@echo off
"%~dp0bin\protoc.exe" -o test.pb test.proto
pause