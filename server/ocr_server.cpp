#include "ocr_server.h"
#include <leptonica/allheaders.h>

OCRServiceImpl::OCRServiceImpl() : pool(4) {}  // 4 worker threads

#include <thread>
#include <atomic>
#include <memory>
#include <queue>
#include <condition_variable>
#include <mutex>

// ... other includes already present (leptonica, tesseract, grpc, your proto headers) ...

grpc::Status OCRServiceImpl::SendImage(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<ocr::OcrResult, ocr::ImageRequest>* stream)
{
    ocr::ImageRequest req;

    // Shared state so worker lambdas can safely push results even if this function
    // outlives some local references. Using shared_ptr extends lifetime.
    auto resultQueue   = std::make_shared<std::queue<ocr::OcrResult>>();
    auto resultMutex   = std::make_shared<std::mutex>();
    auto resultCv      = std::make_shared<std::condition_variable>();
    auto reading       = std::make_shared<std::atomic<bool>>(true);
    auto writeFailure  = std::make_shared<std::atomic<bool>>(false);

    // Writer thread: single thread responsible for all stream->Write calls.
    std::thread writer([stream, context, resultQueue, resultMutex, resultCv, reading, writeFailure]() {
        while (true) {
            std::unique_lock<std::mutex> lk(*resultMutex);
            // Wait until either there is a result or reading finished
            resultCv->wait(lk, [&] {
                return !resultQueue->empty() || !reading->load() || writeFailure->load();
            });

            // If a write failure occurred (client disconnected), stop.
            if (writeFailure->load()) return;

            while (!resultQueue->empty()) {
                ocr::OcrResult res = resultQueue->front();
                resultQueue->pop();
                // release lock while writing
                lk.unlock();

                // Check for cancellation before writing
                if (context->IsCancelled()) {
                    writeFailure->store(true);
                    return;
                }

                // Perform the single-threaded stream write.
                if (!stream->Write(res)) {
                    // Write failed (client disconnected). mark failure and exit.
                    writeFailure->store(true);
                    return;
                }

                lk.lock();
            }

            // If no more incoming readers and queue empty, we can exit.
            if (!reading->load() && resultQueue->empty()) {
                return;
            }
        }
    });

    // Read incoming ImageRequest messages and enqueue OCR tasks.
    while (stream->Read(&req)) {
        // Copy necessary data into strings (safe to capture by value).
        std::string batch = req.batch_id();
        std::string imgId = req.image_id();
        std::string raw   = req.data();

        // If writer already observed a write failure (client gone), stop accepting work.
        if (writeFailure->load()) break;

        // Enqueue OCR work. Capture shared_ptrs by value so they remain valid.
        pool.enqueue([batch, imgId, raw, resultQueue, resultMutex, resultCv, reading, writeFailure]() {
            // If a write failure happened, it's pointless to continue processing.
            if (writeFailure->load()) return;

            // Load image into Leptonica Pix *
            Pix *image = pixReadMem(reinterpret_cast<const unsigned char*>(raw.data()), raw.size());
            if (!image) {
                // push an empty result or error text if desired
                ocr::OcrResult err;
                err.set_batch_id(batch);
                err.set_image_id(imgId);
                err.set_text("[failed to load image]");
                {
                    std::lock_guard<std::mutex> lk(*resultMutex);
                    resultQueue->push(err);
                }
                resultCv->notify_one();
                return;
            }

            // Do Tesseract OCR locally in this worker thread (local TessBaseAPI instance)
            tesseract::TessBaseAPI api;
            if (api.Init(nullptr, "eng") != 0) {
                // Init failed
                ocr::OcrResult err;
                err.set_batch_id(batch);
                err.set_image_id(imgId);
                err.set_text("[tesseract init failed]");
                {
                    std::lock_guard<std::mutex> lk(*resultMutex);
                    resultQueue->push(err);
                }
                resultCv->notify_one();
                pixDestroy(&image);
                return;
            }

            api.SetImage(image);
            char *outText = api.GetUTF8Text();

            ocr::OcrResult res;
            res.set_batch_id(batch);
            res.set_image_id(imgId);
            res.set_text(outText ? outText : "");

            // push result into the queue (thread-safe)
            {
                std::lock_guard<std::mutex> lk(*resultMutex);
                resultQueue->push(res);
            }
            resultCv->notify_one();

            api.End();
            pixDestroy(&image);
            delete[] outText;
        });
    }

    // No more incoming reads. Notify writer thread and wait for it to flush results.
    reading->store(false);
    resultCv->notify_all();
    if (writer.joinable()) writer.join();

    // If a write failure happened, return an appropriate gRPC status.
    if (writeFailure->load()) {
        // The client likely disconnected or cancelled.
        return grpc::Status(grpc::StatusCode::CANCELLED, "client disconnected or write failed");
    }

    return grpc::Status::OK;
}

