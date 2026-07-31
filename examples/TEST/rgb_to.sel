#define basic'* = std'basic'*.
#define io'* = ext'io'*.
#symbol rgb = 255.
#symbol torv = basic'real << 1.

#symbol Program =>
[
    #var a := basic'Boolean.
    class'while::a run: =>
	[	
	    'program'output << "%n%nConversor de RGB para XYZ do Torque%n%n".
	    'program'output << "Escolha uma opção%n".
	    'program'output << "1 - Converter de RGB para o valor do torque.%n2 - Converter do valor do toque para RGB.%n3 - Sair%n".
	    'program'output << "Sua opção é: ".
	    #var aChoice := io'console >> basic'integer.
	    #if aChoice == 1?
	    [
	        'program'output << "%n%nEntre com o valor do R%n".
	        #var r := io'console >> basic'integer.
	        'program'output << "Entre com o valor do G%n".
	        #var g := io'console >> basic'integer.
	        'program'output << "Entre com o valor do B%n".
	        #var b := io'console >> basic'integer.
	
	        #var val1 := torv*r.
	        #var val2 := val1/rgb.
	
	        #var val3 := torv*g.
	        #var val4 := val3/rgb.
	
	        #var val5 := torv*b.
	        #var val6 := val5/rgb.

	        'program'output << "%n%nX: " << val2.
	        'program'output << "%nY: " << val4.
	        'program'output << "%nZ: " << val6.
	        'program'input get.
	    ]
	    | == 2?
	    [
			#var torq := 1000.
	        'program'output << "%n%nEntre com os Valores do XYZ ja convertidos para o Torque.%n".
			'program'output << "Obs: Não coloque os valores com virgula, caso queira converter o valor de 255, %nescreva o valor 1000%n%n".
	        'program'output << "%n%nEntre com o valor do X%n".
	        #var r := io'console >> basic'integer.
	        'program'output << "Entre com os valor do Y%n".
	        #var g := io'console >> basic'integer.
	        'program'output << "Entre com o valor do Z%n".
	        #var b := io'console >> basic'integer.
	
	        #var val1 := rgb*r.
	        #var val2 := val1/torq.
	
	        #var val3 := rgb*g.
	        #var val4 := val3/torq.
	
	        #var val5 := rgb*b.
	        #var val6 := val5/torq.
			

	        'program'output << "%n%nR: " << (val2+1).
	        'program'output << "%nG: " << (val4+1).
	        'program'output << "%nB: " << (val6+1).
	        'program'input get.
	    ].
		a << (aChoice != 3).
	].
].