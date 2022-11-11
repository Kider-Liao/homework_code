@echo off
cd bin
java -classpath .;..\testcases;..\jflex\exceptions.jar;..\jflex\java-cup-11b.jar;..\jflex\jflex-full-1.8.2.jar;..\jflex\complexity.jar LexicalTestDemo
pause
cd ..
@echo on