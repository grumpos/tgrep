@call "%VS100COMNTOOLS%vsvars32.bat" x86

msbuild .\vsproj\tgrep\tgrep.sln /target:Build /p:Configuration=Release