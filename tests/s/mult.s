addi $a0, $zero, 5000000
addi $a1, $zero, 3000000
multu $a0, $a1

li $v0, 34 # print hex
mfhi $a0  # print hi
syscall

li $v0, 11 # print char
li $a0, 0x0a # '\n'
syscall

li $v0, 34 # print hex
li $v0, 1  # print int
li $v0, 36  # print int
mflo $a0   # print lo
syscall
