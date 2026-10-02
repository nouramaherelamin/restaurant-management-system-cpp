@echo off
REM Build with MinGW-w64 (g++ and windres must be on PATH)
windres resource.rc -O coff -o resource.o
if errorlevel 1 (
  echo windres failed - building without the manifest ^(classic control look^)
  g++ -std=c++17 -O2 -Wall -mwindows -static restaurant_management_gui.cpp -o RestaurantManagement.exe -lcomctl32 -lgdi32
) else (
  g++ -std=c++17 -O2 -Wall -mwindows -static restaurant_management_gui.cpp resource.o -o RestaurantManagement.exe -lcomctl32 -lgdi32
)
if errorlevel 1 ( echo BUILD FAILED ) else ( echo Built RestaurantManagement.exe )
