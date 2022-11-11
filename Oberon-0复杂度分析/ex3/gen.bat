@echo off
cd src
java -jar ..\javacup\java-cup-11b.jar -parser Parser -symbols Sym -expect 100 oberon.cup
cd ..
pause