# AC-5 Contiguous rejections negative test
schema 1
kind contiguous
memory_size 100
negative_test 1

alloc P1 a1 30
alloc P1 a2 30
free P1 a1
free P1 a1
free P2 a2
free P1 a99
