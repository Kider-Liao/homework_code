@echo off
chcp 65001
cd src
javadoc -private -author -version -d ..\doc -classpath .;..\testcases;..\jflex\exceptions.jar;..\jflex\java-cup-11b.jar;..\jflex\jflex-full-1.8.2.jar;..\jflex\complexity.jar *.java
cd ..
pause
@echo on