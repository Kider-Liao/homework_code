@echo off
cd bin
java -classpath .;..\javacup\exceptions.jar;..\javacup\java-cup-11b.jar;..\javacup\jflex-full-1.8.2.jar;..\javacup\complexity.jar ComplexityDemo
pause
cd ..
@echo on