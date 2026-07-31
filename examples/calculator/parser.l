// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define ctrl'*  = std'patterns'*.
#define prop'*  = std'properties'*.

#define math'*  = calc'math'*.
#define calc'*  = calc'control'*.
#define parser'* = calc'parser'*.

// --- compiler hints ---
#define[const] LiteralExtension = basic'LiteralExtension.
#define[const] RealExtension    = basic'RealExtension.

// --- ParsingOrders ---

#symbol ParsingOrder0 =
{
    $getLevel = 0.
}.

#symbol ParsingOrder1 =
{
    $getLevel = 1.
}.

#symbol ParsingOrder2 =
{
    $getLevel = 2.
}.

// --- NumericExtension ---

#symbol NumericExtension =
{
    validate : aChar
    [
        basic'True if:((48 <= aChar)and:(57 >= aChar)) | if:(46 == aChar).
    ]
    
    $getExtension = $self.

    $createNode
    [
        ^ math'Number::self.
    ]
}.

// --- OperatorExtension ---

#symbol OperatorExtension =
{
    validate : aChar 
    [
        basic'True if:(43 == aChar) | if:(45 == aChar) | if:(42 == aChar) | if:(47 == aChar).
    ]

    $getExtension = $self.

    $createNode
    [
        #var aToken := basic'LiteralType of:self.

        ^ ctrl'SearchAgent::calc'Operations proceed:{ == anItem = (anItem == aToken). } create:aToken.
    ]
}.

// --- SubExpressionExtension ---

#class SubExpressionExtension
{
    #field theCounter.

    #role Closed
    {
        #method validate [ self fail. ]
    }

    #method new
    [
        theCounter := basic'Integer << 1.
    ]

    #method $createNode
    [
        #var aToken := basic'LiteralType of:self.

        #var aSubExpression := basic'SubString::{for = aToken. from = 1. till = aToken Length - 2.}.

        ^ math'Number::parser'ExpressionFactory::aSubExpression.
    ]

    #method validate : aChar 
    [
        #if("(" == aChar)?
            [ theCounter += 1. ].

        #if(")" == aChar)?
        [ 
            theCounter -= 1. 
            #if(theCounter==0)?
                [ #shift Closed. ].
        ].
    ]
}

// --- BracketExtension ---

#symbol BracketExtension =
{
    validate : aChar 
    [
        basic'True if:("(" == aChar).
    ]

    $getExtension = SubExpressionExtension.
}.

// --- Token ---

#class Token
{
    #field theToken.
    #field theExtension.

    #role Unrecognized
    {
        #method $tryIgnore []

        #method validate : aChar
        [
            theExtension := (ctrl'SearchAgent::calc'TokenExtensions proceed:
            { 
                == aValidator 
                [
                    ^ aValidator validate:aChar back:basic'True | back:basic'False.
                ] 
            }) $getExtension.

            #shift.
        ]
    }

    #method basic'$toLiteral = basic'LiteralType of:theToken.

    #method new
    [
        theToken := basic'String.
        theExtension := nil.

        #shift Unrecognized.
    ]

    #method += aChar
    [
        #if(32 == aChar)?
            [ ^ self $tryIgnore. ].

        #if self validate:aChar
        [
            theToken += aChar.
        ]
        | [ self fail. ].
    ]

    #method clear
    [
        theToken clear.

        #shift Unrecognized.
    ]

    #annex(#group(theExtension, LiteralExtension)).
}

// --- ExpressionNode ---

#class ExpressionNode
{
    #field theNode.

    #field theLeft.
    #field theRight.

    #role RightAssigned
    {
        #method X = theLeft.

        #method Y = theRight.
    
	    #method + anExpression
	    [
            #if(self $getLevel < anExpression $getLevel)
                ? [ ^ (anExpression + self). ]
                | ! [ theRight := theRight + anExpression. ].
        ]
    }

    #role LeftAssigned
    {
        #method X = theLeft.

	    #method + anExpression
	    [
	        #if(self $getLevel < anExpression $getLevel)
	            ? [ ^ (anExpression + self). ]
	            | ! [
                    theRight := anExpression.

                    self $setRightAssigned.
                ].
        ]
    }

    #method $setRightAssigned [ #shift RightAssigned. ]

    #method + anExpression
    [
        basic'False if:((self $getLevel == 0)and:(anExpression $getLevel == 0)).

        #if(self $getLevel < anExpression $getLevel)
            ? [ ^ (anExpression + self). ]
            | ! [
                theLeft := anExpression.

                #shift LeftAssigned.
            ].
    ]

    #method new : aNode
    [
        theNode := aNode.
    ]

    #annex(theNode).
}

// --- ExpressionTree ---

#class ExpressionTree
{
    #field theRoot.

    #role Empty
    {
        #method += aNode
        [
            theRoot := aNode.

            #shift.
        ]
    }

    #method basic'$toReal = theRoot $invoke.

    #method new
    [
        #shift Empty.
    ]

    #method += aNode
    [
        theRoot := ExpressionNode::aNode + theRoot.
    ]

    #annex(RealExtension).
}

// --- Parsing ---

#class Parsing
{
    #field theToken.
    #field theTree.

    #method new
    [
        theToken := Token.
        theTree := ExpressionTree.
    ]

    #method basic'$toReal
    [
        theTree += theToken $createNode.

        ^ theTree $invoke.
    ]

    #method $newToken : aChar
    [
        basic'True if:(theToken Length > 0).

        theTree += theToken $createNode.

         theToken clear += aChar.
    ]

    #method proceed : aChar
    [
        ctrl'Control if:(theToken += aChar) | if:(self $newToken:aChar).
    ]

    #annex(RealExtension).
}

// --- ExpressionFactory ---

#symbol ExpressionFactory : anExpression
    = ctrl'Control run: =>
    [
        #var aLiteral := basic'LiteralType of:anExpression.

        #if("-" == aLiteral@0)?
        [
            aLiteral := "0" + aLiteral.
        ].

        ^ basic'VariantValue::(basic'RealType of:(prop'Enumerator::aLiteral run:Parsing)).
    ]
    | back:"Invalid expression".
