/*
:name: if
:description: A module testing if statement
:should_fail: 0
:tags: 12.4
*/
module if_tb ();
	wire a = 0;
	reg b = 0;
	always begin
		if(a) b = 1;
	end
endmodule
