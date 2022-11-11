@echo off
cd bin
java -classpath .;..\jar\exceptions.jar;..\jar\java-cup-11b.jar;..\jar\jflex-full-1.8.2.jar;..\jar\complexity.jar Main
pause
cd ..
@echo on