@echo off
cd bin
java -classpath .;..\jflex\exceptions.jar;..\jflex\java-cup-11b.jar;..\jflex\jflex-full-1.8.2.jar;..\jflex\complexity.jar LexicalMainDemo
pause
cd ..
@echo on