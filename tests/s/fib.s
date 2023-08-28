.text
	# a0 should be loaded by the tests (see tests/runtime_test.c)
	# If a0 has not been loaded then just load a value into a0
	bne		$a0, $zero, do_fib
	li		$a0, 9
do_fib:

	jal		fib
	move	$s0, $v0    # Tests rely on $s0 having the result (tests/runtime_test.c)

	addi	$a0, $v0, 0
	addi	$v0, $zero, 1
	syscall

	# print '\n'
	addi	$a0, $zero, 0xa
	addi	$v0, $zero, 11
	syscall

	addi	$v0, $zero, 10 # syscall exit
	syscall                # exit(0)
	j		stop           # just in case my syscalls don't work

# int fib(n) {
# 	if (n == 0) return 0;
# 	if (n == 1) return 1;
# 	return fib(n - 1) + fib(n - 2);
# }
fib:
	addi	$sp, $sp, -12
	sw		$ra, 0($sp)
	sw		$s0, 4($sp)
	sw		$s1, 8($sp)

	add		$s0, $a0, $zero       # s0 = n
	addi	$t1, $zero, 1         # t1 = 1
	beq		$s0, $zero, _fib_ret0 # if (s0 == 0) return 0
	beq		$s0, $t1, _fib_ret1   # if (s0 == 1) return 1

	# return fib(n - 1) + fib(n - 2)
	addi	$a0, $s0, -1
	jal		fib
	add		$s1, $zero, $v0 # s1 = fib(n - 1)
	addi	$a0, $s0, -2    # v0 = fib(n - 2)
	jal		fib
	add		$v0, $s1, $v0 # return s1 + v0

  _fib_exit:
	# pop stack and return
 	lw		$ra, 0($sp)
 	lw		$s0, 4($sp)
 	lw 		$s1, 8($sp)
	addi	$sp, $sp, 12
 	jr		$ra

  _fib_ret0:
 	addi	$v0, $zero, 0
 	j		_fib_exit
  _fib_ret1:
 	addi	$v0, $zero, 1
 	j		_fib_exit

stop: