#define propts'*   = std'properties'*.
#define basic'*    = std'basic'*.
#define forms'*    = gui'forms'*.
#define prop'*     = gui'controls'properties'*.
#define graph'*    = gui'graphics'*.
#define controls'* = gui'controls'*.
#define events'*   = sys'events'*.

#class window (forms'SDIDialog)
{
    #field pnlFormula.
    #field pnlGrafico.
    #field valUm.
    #field valDois.
    #field BtmCalc.
	#field BtmSair.
    #field valor.
    #field theImageList.
    #field thePaintbox.
    #field Tconta.
	#field Econta.

    #method new
    [ super new. ]

    #method forms'$initialize
    [
        pnlFormula   := controls'Panel::self.
        pnlGrafico   := controls'Panel::self.
        valUM        := controls'edit::self.
        valDois      := controls'edit::self.
        BtmCalc      := controls'Button::self.
		BtmSair      := controls'Button::self.
        valor        := controls'staticlabel::self.
        thePaintbox  := controls'Paintbox::pnlFormula.
        theImageList := graph'ImageList::pnlFormula.
        Tconta       := controls'radioButtonGroup::self.    
		Econta       := controls'radioButtonGroup::self.

        super $invoke.

        prop'Caption::self << "Calc Area GUI".
        prop'Size::self << graph'Size{ Width=600. Height=400. }.
        prop'Location::self << graph'Point{ X=250. Y=200. }.

        prop'Caption::pnlFormula << "Formula".
        prop'Location::pnlFormula << graph'Point{ X=3. Y=242. }.
        prop'Size::pnlFormula << graph'Size{ Width=130. Height=130. }.
        prop'Visible::pnlFormula << basic'True.

        prop'Caption::pnlGrafico << "Grafico".
        prop'Location::pnlGrafico << graph'Point{ X=420. Y=242. }.
        prop'Size::pnlGrafico << graph'Size{ Width=172. Height=130. }.
        prop'Visible::pnlGrafico << basic'True.

        prop'Caption::Tconta << " Escolha do calculo ".
        prop'Items::Tconta += "&Paralelogramos"  += "&Trapézio" += "&Círculos".
        prop'Location::Tconta << graph'Point{ X=7. Y=1. }.
        prop'Size::Tconta << graph'Size{ Width=150. Height=80. }.
        prop'Visible::Tconta << basic'True. 

		prop'Caption::Econta << " Escolha do calculo ".
        prop'Location::Econta << graph'Point{ X=7. Y=90. }.
        prop'Size::Econta << graph'Size{ Width=150. Height=100. }.

     	prop'Caption::valUM << "30".
        prop'Location::valUM << graph'Point{ X=170. Y=10. }.
        prop'Size::valUM << graph'Size{ Width=150. Height=22. }.
        prop'Visible::valUM << basic'True.

      	prop'Caption::valDois << "20".
        prop'Location::valDois << graph'Point{ X=170. Y=40. }.
        prop'Size::valDois << graph'Size{ Width=150. Height=22. }.
        prop'Visible::valDois << basic'True.

        prop'Caption::BtmCalc << "Calcular".
        prop'Location::BtmCalc << graph'Point{ X=430. Y=10. }.
        prop'Size::BtmCalc << graph'Size{ Width=150. Height=22. }.
        prop'Visible::BtmCalc << basic'True.

        prop'Caption::BtmSair << "Sair".
        prop'Location::BtmSair << graph'Point{ X=430. Y=40. }.
        prop'Size::BtmSair << graph'Size{ Width=150. Height=22. }.
        prop'Visible::BtmSair << basic'True.

		prop'Caption::valor << "Resposta: ".
        prop'Location::valor << graph'Point{ X=160. Y=300. }.
        prop'Size::valor << graph'Size{ Width=150. Height=22. }.
        prop'Visible::valor << basic'True.

        prop'Size::thePaintbox << graph'Size { width=600. Height=400.}.
        prop'Location:: thePaintbox <<graph'Point {X = 0. Y = 0.}.
        prop'Visible:: thePaintbox <<basic'true.

        events'Handlers::thePaintbox +=
            { controls'$onRender : aHandle = self $onPaint:aHandle. }.

        events'Handlers::Tconta += 
            { controls'$OnIndexChanged : anIndex = self $TipoDoCalculo:anIndex. }.
		
		events'Handlers::Econta += 
            { controls'$OnIndexChanged : anInicio = self $TipoDoObj:anInicio. }.

		events'Handlers::BtmCalc +=
            { controls'$onClick = self $onButtonClick. }.

		events'Handlers::BtmSair +=
            { controls'$onClick [ 'program exit. ] }.
    ]

    #method $onPaint : aCanvas
    [
        #var anInicio := basic'IntegerType of:Econta.
        #var aDrawing := graph'Drawing::aCanvas.
        aDrawing << graph'Point { X=10. Y=30. }.   
        aDrawing proceed:(theImageList@anInicio).
    ]

    #method $TipoDoCalculo : anIndex
    [   
        #if anIndex ==0?
        [
	        theImageList += "formulas\Paralelogramos\bitmap1.bmp".
	        theImageList += "formulas\Paralelogramos\bitmap2.bmp".
	        theImageList += "formulas\Paralelogramos\bitmap3.bmp".
			theImageList += "formulas\Paralelogramos\bitmap4.bmp".

			prop'Items::Econta += "&Retângulo"  += "&Quadrado" += "&Paralelogramo" += "&Losango".      
			prop'Visible::Econta << basic'True. 
        ]
        | == 1?
        [           
	        theImageList += "formulas\Trapezio\bitmap1.bmp".
	        theImageList += "formulas\Trapezio\bitmap2.bmp".
	        theImageList += "formulas\Trapezio\bitmap3.bmp".

			prop'Items::Econta += "&Retângulo"  += "&Isósceles" += "&Escaleno".
			prop'Visible::Econta << basic'True. 
        ]
        | == 2?
        [
	        theImageList += "formulas\Circulo\bitmap1.bmp".
	        theImageList += "formulas\Circulo\bitmap2.bmp".
	        theImageList += "formulas\Circulo\bitmap3.bmp".

			prop'Items::Econta += "&Círculo"  += "&Coroa circular" += "&Setor circular".
			prop'Visible::Econta << basic'True.
        ].

    ]
	
	#method $TipoDoObj : anInicio
	[
		#if anInicio == 0?
		[
		]
		| == 1?
		[
		]
		| == 2?
		[
		]
		| == 3?
		[
		]
		| == 4?
		[
		].
		thePaintbox refresh.
	]
	
	#method $onButtonClick
	[
		#var B := basic'LiteralType of:valUM.
		#var H := basic'LiteralType of:valDois.
        #var aResult := basic'IntegerType of:(basic'Variant::valUM) * basic'IntegerType of:(basic'Variant::valDois) / 2.
		prop'Caption::valor << "Resposta: " + basic'Variant::aResult.
	]
}