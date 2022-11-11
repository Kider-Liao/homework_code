@echo off
cd src
javac -d ..\bin -classpath .;..\jflex\exceptions.jar;..\jflex\java-cup-11b.jar;..\jflex\jflex-full-1.8.2.jar;..\jflex\complexity.jar *.java
cd ..
pause