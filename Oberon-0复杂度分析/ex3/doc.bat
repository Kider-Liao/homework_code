@echo off
chcp 65001
cd src
javadoc -private -author -version -d ..\doc -classpath .;..\testcases;..\javacup\exceptions.jar;..\javacup\java-cup-11b.jar;..\javacup\jflex-full-1.8.2.jar;..\javacup\complexity.jar *.java
cd ..
pause
@echo on