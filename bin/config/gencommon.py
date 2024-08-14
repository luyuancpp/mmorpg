#!/usr/bin/env python
# coding=utf-8

beginrowidx = 7

def mywrite(str, filename):
        outputh = open(filename, "w", encoding="utf-8")
        outputh.write(str)
        outputh.close()
