.text
	#addi $sp, $sp, -40  # int array[10];

	li $t0, 0  # i = 0
	li $t1, 10 # end = 10
load_loop:
	addi $t2, $zero, 4
	sub $t2, $sp, $t0 # t2 = &stack[i]
	lw $t0, 0($t2) # array[i] = i

	addi $t0, $t0, 1  # i++
	bne	$t0, $t1, load_loop # while(i != 10)

	move $a0, $sp
	addi $sp, $sp, -40
	j end
	
	
	
	
	
	
end: