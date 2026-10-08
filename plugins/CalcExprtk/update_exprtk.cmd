@echo off
cd /d "%~dp0..\.."
echo Updating exprtk submodule...
git submodule update --remote 3rdparty/ExprTk
echo Done!
