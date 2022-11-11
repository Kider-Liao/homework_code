@echo off
chcp 65001
cd src
javac -d ..\bin -classpath .;..\javacup\exceptions.jar;..\javacup\java-cup-11b.jar;..\javacup\jflex-full-1.8.2.jar;..\javacup\complexity.jar *.java -Xlint:deprecation
cd ..
pause