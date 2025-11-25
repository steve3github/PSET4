first build

cmake -S .. -B . -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/Users/user/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release


Once built 

To run the server
.\Release\server.exe

To run the client
.\Release\client.exe