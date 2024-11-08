Remove-Item -r -fo build
mkdir build
Set-Location build
cmake .. -G "Unix Makefiles"
mingw32-make
mingw32-make install