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
    std::atomic<bool> reading(true);
    std::atomic<bool> writerFailed(false);

    std::mutex resultMutex;
    std::condition_variable resultCv;
    std::queue<ocr::OcrResult> resultQueue;

    // ---------------------------
    //  WRITER THREAD
    // ---------------------------
    std::thread writer([&]() {
        while (true) {
            std::unique_lock<std::mutex> lock(resultMutex);

            resultCv.wait(lock, [&]() {
                return !resultQueue.empty() || (!reading && resultQueue.empty());
            });

            if (resultQueue.empty() && !reading)
                return; // nothing left to write

            ocr::OcrResult res = resultQueue.front();
            resultQueue.pop();
            lock.unlock();

            // Attempt to write. If the client disconnected, this will fail.
            if (!stream->Write(res)) {
                writerFailed = true;
                return;
            }
        }
    });


    ocr::ImageRequest req;

    // ---------------------------
    //  READER LOOP
    // ---------------------------
    while (stream->Read(&req)) {
        std::string batch = req.batch_id();
        std::string imgId = req.image_id();
        std::string raw = req.data();

        pool.enqueue([batch, imgId, raw,
                      &resultMutex, &resultCv, &resultQueue,
                      &writerFailed]() {

            if (writerFailed) return;

            ocr::OcrResult result;
            result.set_batch_id(batch);
            result.set_image_id(imgId);

            try {
                // 1. Load image safely
                Pix* image = pixReadMem(reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
                if (!image) {
                    result.set_text("[ERROR] Invalid or corrupted image.");
                } else {

                    // 2. Try OCR
                    try {
                        tesseract::TessBaseAPI api;
                        if (api.Init(nullptr, "eng")) {
                            result.set_text("[ERROR] Failed to initialize Tesseract.");
                        } else {
                            api.SetImage(image);

                            char* outText = api.GetUTF8Text();
                            if (outText) {
                                result.set_text(outText);
                                delete[] outText;
                            } else {
                                result.set_text("[ERROR] Tesseract OCR failed.");
                            }

                            api.End();
                        }
                    } catch (const std::exception& e) {
                        result.set_text(std::string("[ERROR] OCR exception: ") + e.what());
                    }

                    pixDestroy(&image);
                }
            }
            catch (const std::exception& e) {
                result.set_text(std::string("[FATAL ERROR] Exception: ") + e.what());
            }
            catch (...) {
                result.set_text("[FATAL ERROR] Unknown exception.");
            }

            // Do not enqueue result if writer already died
            if (writerFailed) return;

            // ---------------------------
            //  PUSH RESULT INTO QUEUE
            // ---------------------------
            {
                std::lock_guard<std::mutex> lock(resultMutex);
                resultQueue.push(result);
            }
            resultCv.notify_one();
        });
    }

    // Reader finished
    reading = false;
    resultCv.notify_all();
    writer.join();

    return grpc::Status::OK;
}


