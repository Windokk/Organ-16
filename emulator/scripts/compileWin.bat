cd ..
cmake -S . -B build -G "MinGW Makefiles" -DQt6_DIR="C:/Qt/6.9.2/mingw_64/lib/cmake/Qt6/"
cd build
cmake --build .