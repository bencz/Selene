
// --- Prompt ---

#symbol IntSamplePrompt = "Enter the two integer numbers:%n".

#symbol RealSamplePrompt = "Enter the two real numbers:%n".

// --- Reader Helper ---

#symbol NumberReader = 
    aNumber => 
    [
        #if 'program'input >> aNumber
            | [ 'program'output << "Input error. Aborting%n". $self fail. ].
    ].
    
// --- Program ---

#symbol Program =>
[
    'program'output << 'SumSample'Prompt.

    #var a := 'SumSample'Number.
    #var b := 'SumSample'Number.

    NumberReader proceed:a proceed:b.

    'program'output << "The sum is " << (a + b) << "%n".
    
    'program'input get. // wait for any key
].