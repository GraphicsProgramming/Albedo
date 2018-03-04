@echo off

echo ===== CLEANING  =====
rmdir build /s /q

echo ===== COMPILING =====
call compile.bat

echo =====  RUNNING  =====
call run.bat