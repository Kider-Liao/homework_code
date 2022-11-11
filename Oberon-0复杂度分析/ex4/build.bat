@echo off
chcp 65001
cd src
javac -d ..\bin -classpath .;..\jar\exceptions.jar;..\jar\java-cup-11b.jar;..\jar\jflex-full-1.8.2.jar;..\jar\complexity.jar *.java -Xlint:deprecation
cd ..
pause