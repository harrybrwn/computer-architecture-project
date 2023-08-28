	addi $t1, $zero, 0
	addi $a0, $zero, 5
	addi $a1, $zero, 0

	bne	 $a0, $zero, label1
	addi $t1, $t1, 1
	addi $t1, $t1, 1
	addi $t1, $t1, 1
	addi $t1, $t1, 1
label1:

	beq  $a1, $zero, label2
	addi $t1, $t1, 1
	addi $t1, $t1, 1
	addi $t1, $t1, 1
label2:

# Register $t1 should still be zero