.text
addiu	$a0, $zero, -9
addiu	$a1, $zero, 7
addu	$t1, $a0, $a1

addu $a0, $a0, $a1
li $v0, 34
syscall
