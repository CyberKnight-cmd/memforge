# AC-5 Paging access violations negative test
schema 1
kind paging
page_size 4096
frames 3
negative_test 1

spawn P0 4
access P0 9
access P9 0
