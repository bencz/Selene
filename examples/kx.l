#define basic'* = std'basic'*.
#define math'*  = std'basic'math'*.
#symbol g = 10.

#symbol Program =>
[
    'program'output << "Escolha uma opção%n".
    'program'output << "1 - Descobrir a Konstante Elastica%n".
    'program'output << "2 - Descobrir a deformação%n".
    #var aChoice := ext'io'console >> basic'integer.

    #if(aChoice == 1)?
    [
        'program'output << "%n%nDigite o valor da massa: ".
        #var m := ext'io'console >> basic'integer.
        'program'output << "Digite o valor da altura: ".
        #var h := ext'io'console >> basic'integer.
        'program'output << "Digite o valor da deformação da mola: ".
        #var x := ext'io'console >> basic'integer.

        #var masgh := m*g*h.
        #var prF := masgh*2.
        #var final := prF/(x*x).
        'program'output << "%nA konstante da sua mola é: " << final.
        'program'input get.
        'program exit.
    ].

    #if(aChoice == 2)?
    [
        'program'output << "Digite o valor da massa: ".
        #var m := ext'io'console >> basic'integer.
        'program'output << "Digite o valor da altura: ".
        #var h := ext'io'console >> basic'integer.
        'program'output << "Digite o valor da Konstante da mola: ".
        #var k := ext'io'console >> basic'integer.

        #var masgh := m*g*h.
        #var prF := masgh*2.
        #var antesF := prF/k.
        #var final := math'sqrt::(antesF).
        'program'output << "A konstante da sua mola é: " << final.
        'program'input get.
        'program exit.
    ].
].