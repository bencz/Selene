#define basic'* = std'basic'*.

#define forms'*    = gui'forms'*.
#define gui'prop'* = gui'controls'properties'*.
#define graph'*    = gui'graphics'*.
#define controls'* = gui'controls'*.

#define events'*   = sys'events'*.

// --- Main Window --

#class CalcWindow (forms'SDIDialog)
{
    #field theEdit.
    #field theButton.
//    #field theHistory.
//    #field theLabel.
//
//    #role EmptyHistory
//    {
//        #method $onHistoryClick
//        [
//        ]
//
//        #method $addToHistory : anExpression
//        [
//            theHistory->gprop'Items += anExpression.
//
//            #shift.
//        ]              
//    }

    #method new
    [
        super new.

//        #shift EmptyHistory.
    ]

    #method forms'$initialize
    [
        super $invoke.

        theEdit := controls'Edit::self.
        theButton := controls'Button::self.

        events'Handlers::theButton +=
            { controls'$onClick = self $onButtonClick. }.

//        theLabel := self->gcomp'StaticLabel.
//        theHistory := self->gcomp'ListBox.

//        theHistory->sys'events'Handlers +=           
//            { gcomp'$onClick = self::$onHistoryClick. }.

        gui'prop'Caption::self << "Calculator ELENA Sample".
        gui'prop'Size::self << graph'Size{ Width=310. Height=200. }.
        gui'prop'Location::self << graph'Point{ X=50. Y=50. }.

        theEdit << "1+12*3-14".
        gui'prop'Size::theEdit << graph'Size{ Width=200. Height=24. }.
        gui'prop'Location::theEdit <<  graph'Point{ X=10. Y=10. }.
        gui'prop'Visible::theEdit << basic'true.

        gui'prop'Caption::theButton << "Evaluate".
        gui'prop'Size::theButton << graph'Size{ Width=70. Height=30. }.
        gui'prop'Location::theButton << graph'Point{ X=220. Y=10. }.
        gui'prop'Visible::theButton << basic'true.

//        theLabel->gprop'Size << gprop'Point{ X=200. Y=20. }.
//        theLabel->gprop'Location << gprop'Point{ X=10. Y=45. }.
//        theLabel->gprop'Caption << "History:".
//        theLabel->gprop'Visible << basic'True.
//
//        theHistory->gprop'Size << gprop'Point{ X=200. Y=100. }.
//        theHistory->gprop'Location << gprop'Point{ X=10. Y=60. }.
//        theHistory->gprop'Visible << basic'True.
    ]

//    #method $addToHistory : anExpression
//    [
//        (theHistory->gprop'Items->vprop'Indexer << { vprop'$Index= 0. }) insert:anExpression.
//    ]
//
//    #method $onHistoryClick
//    [
//        #var anExpression := (theHistory->gprop'Items @ theHistory->gprop'SelectedIndex)->basic'LiteralValue.
//
//        theEdit << anExpression.
//    ]

	#method $onButtonClick
	[
		#var anExpression := basic'LiteralType of:theEdit.

        //self $addToHistory:anExpression.

        #var aResult := calc'Parser'ExpressionFactory::anExpression.

		theEdit << aResult.
	]
}