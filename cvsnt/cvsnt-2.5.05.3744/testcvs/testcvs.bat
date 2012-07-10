@echo off
rmdir /s /q tree tree_0 repos repos_0
python testcvs.py %1 %2 %3 %4
