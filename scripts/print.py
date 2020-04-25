#!/usr/bin/env python3

expect = [0, 1]
result = []
result_split = []
dics = []



for i in range(2, 6000000):
    tmp = expect[0] + expect[1]
    expect[0] = expect[1]
    expect[1] = tmp

print("fibo[4294967295] : %x " % expect[1])

