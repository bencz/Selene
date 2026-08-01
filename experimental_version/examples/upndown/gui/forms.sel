// --- namespace shortcuts ---
#define basic'*         = std'basic'*.
#define prop'*          = std'properties'*.
#define ctrl'*          = std'patterns'*.
#define coll'*          = std'collections'*.
#define events'*        = sys'events'*.

#define forms'*         = gui'forms'*.
#define controls'*      = gui'controls'*.
#define gui'prop'*      = gui'controls'properties'*.
#define graph'*         = gui'graphics'*.

#define sockets'*       = win32'socket'controls'*.

#define gui'*           = cardgame'gui'forms'*.
#define net'*           = cardgame'net'*.
#define config'*        = cardgame'configs'*.
#define engine'*        = cardgame'engine'*.
#define players'forms'* = cardgame'players'forms'*.
//#define upndown'*       = cardgame'game'upndown'*.
#define players'*       = cardgame'players'*.
#define game'*          = cardgame'game'*.

#define[const]IndexerExtension = prop'IndexerExtension.
#define[const]ListExtension    = coll'ListExtension.

// === Constants ===

#symbol hndShift = 25.

#symbol hndLeft  = 32.
#symbol hndTop   = 295.

#symbol trmLeft  = 11.
#symbol trmTop   = 88.

#static tblCoords = 
( 
    graph'Point {X=162. Y=216.}, 
    graph'Point {X=112. Y=192.} , 
    graph'Point {X=80.  Y=232.},
    graph'Point {X=128. Y=256.}
).

// --- Module ---

#symbol Module =
{
    proceed
    [
        config'ClientPlayerExtensionFactories += players'forms'ClientExtensionFactory.

        config'HostPlayerExtensionFactories += players'forms'HostExtensionFactory.
    ]
}.

// --- Hand ---

#class Hand
{
    #field theCards.
    #field theCount.

    #method new : aCardnumber
    [
        theCount := basic'Integer << 0.
        theCards := basic'ArrayType create:{ Length = aCardnumber. Create = basic'Integer. }.
    ]

    #method clear
    [
        theCount << 0.

        prop'Enumerator::theCards run: aCard =>(aCard << 0).
    ]

    #method += aCard
    [
        #var aMiddle := theCards Count / 2.
/*
        ctrl'ScopeEnumerator { for=theCards. from=aMiddle - theCount. till=aMiddle. } run: anItem =>
        [
            theCards@(prop'Index::anItem - 1) << anItem.
        ].
*/
        theCount += 1.

        theCards@aMiddle << aCard.
    ]

    #method -= aCard
    [
        //prop'Content(ctrl'SearchAgent::theCards proceed:aCard) set:0.
    ]

    #method prop'$getEnumerator = theCards $invoke.

    #annex (ListExtension, IndexerExtension).
}

// --- GameVariants
#symbol ServerGame = { ? [] $isServerGame [] }.
#symbol ClientGame = { ? [] }.

// --- Connector ---

#class Connector (forms'Dialog)
{
    // --- Controls ---
    #field cmdQuit.
	#field cmdStart.
	#field cmdLoad.
	#field cmdConnect.
	#field cmdSearch.
	#field cmdAccept.
    #field cmdDeny.
	#field cmdShowStat.

    #field rbgConnectType.
	#field pnlClient.
	#field pnlHost.
	#field txtUserName.
    #field txtPassword.
	#field NoConnectNotification.
	#field ConnectingNotification.
	#field ConnectNotification.
	#field HostName.
	#field HostIP.
	#field GameVariant.
	#field PlayersPanel.
	#field SPLabel.
    #field TPLabel.
    #field FPLabel.
    #field SecondPlayer.
    #field ThirdPlayer.
    #field FourthPlayer.
	#field Label5.
    #field GroupBox1.
    #field GroupBox2.
    #field Label1.
    #field Label2.

    #field theCounter.
    #field thePlayer.

	#method new : aParent
	[
		super new:aParent.

        theCounter := basic'Integer << 0.
    ]

    #method forms'$initialize
    [
        // --- Create controls ---
        cmdLoad   := controls'Button::self.
        cmdQuit   := controls'Button::self.
        cmdStart  := controls'Button::self.
		pnlClient := controls'Panel::self.
		pnlHost   := controls'Panel::self.
        rbgConnectType := controls'radioButtonGroup::self.
		GroupBox1 := controls'Panel::self.

		Label1 := controls'StaticLabel::GroupBox1.
		Label2 := controls'StaticLabel::GroupBox1.
		txtUserName := controls'Edit::GroupBox1.
		txtPassword := controls'Edit::GroupBox1.

        cmdSearch := controls'Button::pnlHost.
		cmdAccept := controls'Button::pnlHost.
		cmdDeny := controls'Button::pnlHost.
		cmdShowStat := controls'Button::pnlHost.

        cmdConnect := controls'Button::pnlClient.
		GroupBox2 := controls'Panel::pnlClient.						
		Label5 := controls'StaticLabel::pnlClient.
		HostIP := controls'Edit::pnlClient.

		NoConnectNotification := controls'StaticLabel::GroupBox2.
		ConnectingNotification := controls'StaticLabel::GroupBox2.
		ConnectNotification := controls'StaticLabel::GroupBox2.
		HostName := controls'StaticLabel::GroupBox2.
        GameVariant := controls'radioButtonGroup::pnlHost.
		PlayersPanel := controls'Panel::pnlHost.

		SPLabel := controls'StaticLabel::PlayersPanel.						
        TPLabel := controls'StaticLabel::PlayersPanel.
		FPLabel := controls'StaticLabel::PlayersPanel.
		SecondPlayer := controls'ComboBox::PlayersPanel.
		ThirdPlayer := controls'ComboBox::PlayersPanel.
		FourthPlayer := controls'ComboBox::PlayersPanel.

        // Form
        gui'prop'Location::self << graph'Point{ X=100. Y=100. }.
        gui'prop'Size::self << graph'Size{ Width=560. Height=375. }.
        gui'prop'Caption::self << "Up'N'Down: Establish connections with other players".

        // cmdLoad
        gui'prop'Caption::cmdLoad << "Load Game".
        gui'prop'Location::cmdLoad << graph'Point{ X=196. Y=322. }.
        gui'prop'Size::cmdLoad << graph'Size{ Width=145. Height=25. }.
        gui'prop'Visible::cmdLoad << basic'True.

        // cmdQuit
        gui'prop'Caption::cmdQuit << "Quit Game".
        gui'prop'Location::cmdQuit << graph'Point{ X=379. Y=322. }.
        gui'prop'Size::cmdQuit << graph'Size{ Width=145. Height=25. }.
        gui'prop'Visible::cmdQuit << basic'True.

        // cmdStart
        gui'prop'Caption::cmdStart << "Start Game".
        gui'prop'Location::cmdStart << graph'Point{ X=13. Y=322. }.
        gui'prop'Size::cmdStart << graph'Size{ Width=145. Height=25. }.
        gui'prop'Visible::cmdStart << basic'True.

		// --- pnlClient
		gui'prop'Caption::pnlClient << " Client panel ".
        gui'prop'Location::pnlClient << graph'Point{ X=7. Y=110. }.
        gui'prop'Size::pnlClient << graph'Size{ Width=522. Height=207. }.

		// --- pnlHost
		gui'prop'Caption::pnlHost << "Host panel".
        gui'prop'Location::pnlHost << graph'Point{ X=7. Y=110. }.
        gui'prop'Size::pnlHost << graph'Size{ Width=522. Height=207. }.
        gui'prop'Visible::pnlHost << basic'True.

		// GroupBox1
		gui'prop'Caption::GroupBox1 << " Player specification ".	
        gui'prop'Location::GroupBox1 << graph'Point{ X=7. Y=62. }.
        gui'prop'Size::GroupBox1 << graph'Size{ Width=522. Height=49. }.
		gui'prop'Visible::GroupBox1 << basic'True.

		// rbgConnectType
        gui'prop'Caption::rbgConnectType << " Begin game as... ".
        gui'prop'Items::rbgConnectType += "&Host"  += "&Client".
        gui'prop'Location::rbgConnectType << graph'Point{ X=7. Y=1. }.
        gui'prop'Size::rbgConnectType << graph'Size{ Width=522. Height=60. }.
        rbgConnectType << 0.
        gui'prop'Visible::rbgConnectType << basic'True.

		// Label1
		gui'prop'Caption::Label1 << "Your Name:".
        gui'prop'Location::Label1 << graph'Point{ X=16. Y=20. }.
        gui'prop'Size::Label1 << graph'Size{ Width=79. Height=20. }.
		gui'prop'Visible::Label1 << basic'True.

		// Label2
		gui'prop'Caption::Label2 << "Your log-in:".
        gui'prop'Location::Label2 << graph'Point{ X=263. Y=19. }.
        gui'prop'Size::Label2 << graph'Size{ Width=95. Height=20. }.
		gui'prop'Visible::Label2 << basic'True.

		// txtUserName
        gui'prop'Location::txtUserName << graph'Point{ X=98. Y=20. }.
        gui'prop'Size::txtUserName << graph'Size{ Width=157. Height=24. }.
		gui'prop'Visible::txtUserName << basic'True.

		// txtPassword
        gui'prop'Location::txtPassword << graph'Point{ X=377. Y=16. }.
        gui'prop'Size::txtPassword << graph'Size{ Width=115. Height=24. }.
		gui'prop'Visible::txtPassword << basic'True.

		// GroupBox2
		gui'prop'Caption::GroupBox2 << " Connection status ".
        gui'prop'Location::GroupBox2 << graph'Point{ X=48. Y=48. }.
        gui'prop'Size::GroupBox2 << graph'Size{ Width=369. Height=105. }.
		gui'prop'Visible::GroupBox2 << basic'True.

		// NoConnectNotification
		gui'prop'Caption::NoConnectNotification << "Client not connected to anyone".
        gui'prop'Location::NoConnectNotification << graph'Point{ X=32. Y=21. }.
        gui'prop'Size::NoConnectNotification << graph'Size{ Width=241. Height=20. }.
		gui'prop'Visible::NoConnectNotification << basic'True.

		// ConnectingNotification
		gui'prop'Caption::ConnectingNotification << "Client searching for any host".
        gui'prop'Location::ConnectingNotification << graph'Point{ X=32. Y=45. }.
        gui'prop'Size::ConnectingNotification << graph'Size{ Width=241. Height=20. }.

		// ConnectNotification
		gui'prop'Caption::ConnectNotification << "Client has connected with:".
        gui'prop'Location::ConnectNotification << graph'Point{ X=32. Y=69. }.
        gui'prop'Size::ConnectNotification << graph'Size{ Width=241. Height=20. }.

		// HostName
		gui'prop'Caption::HostName << "HostName".
        gui'prop'Location::HostName << graph'Point{ X=244. Y=69. }.
        gui'prop'Size::HostName << graph'Size{ Width=73. Height=20. }.

		// Label5
		gui'prop'Caption::Label5 << "Enter Host IP:".
        gui'prop'Location::Label5 << graph'Point{ X=25. Y=22. }.
        gui'prop'Size::Label5 << graph'Size{ Width=95. Height=20. }.
		gui'prop'Visible::Label5 << basic'True.

		// HostIP
        gui'prop'Location::HostIP << graph'Point{ X=124. Y=20. }.
        gui'prop'Size::HostIP << graph'Size{ Width=108. Height=24. }.
        gui'prop'Visible::HostIP << basic'True.

        // cmdConnect
        gui'prop'Caption::cmdConnect << "Try to connect".
        gui'prop'Location::cmdConnect << graph'Point{ X=290. Y=18. }.
        gui'prop'Size::cmdConnect << graph'Size{ Width=177. Height=25. }.
		gui'prop'Visible::cmdConnect << basic'True.

        // cmdSearch
		gui'prop'Caption::cmdSearch << "Search players".
        gui'prop'Location::cmdSearch << graph'Point{ X=300. Y=149. }.
        gui'prop'Size::cmdSearch << graph'Size{ Width=145. Height=25. }.
		gui'prop'Visible::cmdSearch << basic'True.

		// cmdAccept
		gui'prop'Caption::cmdAccept << "Accept players".
        gui'prop'Location::cmdAccept << graph'Point{ X=300. Y=178. }.
        gui'prop'Size::cmdAccept << graph'Size{ Width=145. Height=25. }.
		gui'prop'Visible::cmdAccept << basic'True.
        gui'prop'Enabled::cmdAccept << basic'False.

		// cmdDeny
		gui'prop'Caption::cmdDeny << "Refuse players".
        gui'prop'Location::cmdDeny << graph'Point{ X=300. Y=178. }.
        gui'prop'Size::cmdDeny << graph'Size{ Width=145. Height=25. }.

		// cmdShowStat
        gui'prop'Location::cmdShowStat << graph'Point{ X=7. Y=176. }.
        gui'prop'Size::cmdShowStat << graph'Size{ Width=171. Height=25. }.
		gui'prop'Caption::cmdShowStat << "Show All-Game Statistics".

		// GameVariant
        gui'prop'Caption::GameVariant << " Choose game variant ".
        gui'prop'Items::GameVariant += "Two players" += "Three players" += "Four players".
		GameVariant << 2.
        gui'prop'Location::GameVariant << graph'Point{ X=10. Y=21. }.
        gui'prop'Size::GameVariant << graph'Size{ Width=191. Height=121. }.
		gui'prop'Visible::GameVariant << basic'True.

		// PlayersPanel
		gui'prop'Caption::PlayersPanel << "Choose players".
        gui'prop'Location::PlayersPanel << graph'Point{ X=215. Y=21. }.
        gui'prop'Size::PlayersPanel << graph'Size{ Width=274. Height=122. }.
		gui'prop'Visible::PlayersPanel << basic'True.

		gui'prop'Caption::SPLabel << "Second player".
        gui'prop'Location::SPLabel << graph'Point{ X=14. Y=25. }.
        gui'prop'Size::SPLabel << graph'Size{ Width=100. Height=20. }.
		gui'prop'Visible::SPLabel << basic'True.

		gui'prop'Caption::TPLabel << "Third player".
        gui'prop'Location::TPLabel << graph'Point{ X=14. Y=54. }.
        gui'prop'Size::TPLabel << graph'Size{ Width=100. Height=20. }.
		gui'prop'Visible::TPLabel <<  basic'True.

		gui'prop'Caption::FPLabel << "Fourth player".
        gui'prop'Location::FPLabel << graph'Point{ X=14. Y=85. }.
        gui'prop'Size::FPLabel << graph'Size{ Width=100. Height=20. }.
		gui'prop'Visible::FPLabel << basic'True.

		// SecondPlayer
        gui'prop'Location::SecondPlayer << graph'Point{ X=122. Y=25. }.
        gui'prop'Size::SecondPlayer << graph'Size{ Width=141. Height=120. }.
        gui'prop'Visible::SecondPlayer << basic'True.

		// ThirdPlayer
        gui'prop'Location::ThirdPlayer << graph'Point{ X=122. Y=54. }.
        gui'prop'Size::ThirdPlayer << graph'Size{ Width=141. Height=129. }.
		gui'prop'Visible::ThirdPlayer << basic'True.

		// FourthPlayer
        gui'prop'Location::FourthPlayer << graph'Point{ X=122. Y=85. }.
        gui'prop'Size::FourthPlayer << graph'Size{ Width=141. Height=120. }.
		gui'prop'Visible::FourthPlayer << basic'True.

        // event handlers		
        events'Handlers::cmdQuit +=
        { 
            controls'$onClick = gui'prop'DialogResult::self << basic'false. 
        }.
        events'Handlers::rbgConnectType += 
        {
            controls'$OnIndexChanged : anIndex = self $onConnectTypeChange:anIndex. 
        }.
        events'Handlers::GameVariant +=
        {
            controls'$OnIndexChanged : anIndex = self $onGameVariantChange:anIndex. 
        }.
        events'Handlers::cmdSearch +=
        {
            controls'$onClick = self $searchClients. 
        }.
        events'Handlers::cmdStart +=
        {
            controls'$onClick = self $startGame. 
        }.
        events'Handlers::cmdConnect +=
        {
            controls'$onClick = self $connectToServer. 
        }.        
        events'Handlers::cmdAccept +=
        {
            controls'$onClick = self $acceptPlayers. 
        }.        
        events'Handlers::cmdDeny +=
        {
            controls'$onClick = self denyPlayers. 
        }.

        // temporal
        txtUserName << "s".
        txtPassword << "p".
        HostIP << "192.168.229.1".

        super $invoke.
    ]

    #method $onConnectTypeChange : anIndex
    [
        #if anIndex
            ==0 ?
            [
                gui'prop'Visible::pnlClient << basic'False. 
                gui'prop'Visible::pnlHost << basic'True. 
                gui'prop'Visible::cmdStart << basic'True.
                gui'prop'Visible::cmdLoad << basic'True.
            ]
            |
            ==1 ?
            [
                gui'prop'Visible::pnlClient << basic'True.
                gui'prop'Visible::pnlHost << basic'False. 
                gui'prop'Visible::cmdStart << basic'False.
                gui'prop'Visible::cmdLoad << basic'False.
            ].
    ]

    #method $onGameVariantChange : anIndex
    [
        #if anIndex
            ==0 ?
            [
                gui'prop'Visible::ThirdPlayer << basic'False.
                gui'prop'Visible::TPLabel << basic'False.
                gui'prop'Visible::FourthPlayer << basic'False.
                gui'prop'Visible::FPLabel << basic'False.
            ]
            | ==1 ?
            [
                gui'prop'Visible::ThirdPlayer << basic'True.
                gui'prop'Visible::TPLabel << basic'True.
                gui'prop'Visible::FourthPlayer << basic'False.
                gui'prop'Visible::FPLabel << basic'False.
            ]
            | ==2 ?
            [
                gui'prop'Visible::ThirdPlayer << basic'True.
                gui'prop'Visible::TPLabel << basic'True.
                gui'prop'Visible::FourthPlayer << basic'True.
                gui'prop'Visible::FPLabel << basic'True.
            ].
    ]

    #method $searchClients
    [
        #if(basic'LiteralType of:txtUserName Length != 0)?
        [
            theCounter += 1.

            thePlayer := players'Human::
            { 
                config'$getFactoryCollection = 'players'HostExtensionFactories.
                players'$getID = theCounter clone.
                players'$getName=txtUserName. 
                players'$getSocket = theParent $getServerSocket.
                players'$getForm = theParent.
            }.

            theParent $startSearch.

            gui'prop'Enabled::cmdAccept << basic'True.
        ].
    ]

    #method $connectToServer
    [
        #var aUser := basic'LiteralType of:txtUserName.
        #var anIP := basic'LiteralType of:HostIP.

        #if((aUser Length!=0)and:(anIP Length != 0))?
        [
            theParent $connectToServer:anIP.
    
            self $setClientConnectingStatus.
        ].
    ]

	#method $setClientConnectingStatus
	[
        gui'prop'Visible::NoConnectNotification << basic'False.
        gui'prop'Visible::ConnectingNotification << basic'True.
    	gui'prop'Visible::ConnectNotification << basic'False.
        gui'prop'Visible::HostName << basic'False.
	]

    #method $setClientConnectedStatus
    [
        gui'prop'Visible::NoConnectNotification << basic'False.
        gui'prop'Visible::ConnectingNotification << basic'False.
    	gui'prop'Visible::ConnectNotification << basic'True.
        gui'prop'Visible::HostName << basic'True.
    ]

    #method $setDisconnectedStatus
    [
        gui'prop'Visible::NoConnectNotification << basic'True.
        gui'prop'Visible::ConnectingNotification << basic'False.
    	gui'prop'Visible::ConnectNotification << basic'False.
        gui'prop'Visible::HostName << basic'False.
    ]

    #method $onDisconnect
    [
        self $setDisconnectedStatus.
    ]

    #method $onConnect
    [
        gui'prop'Caption::HostName << "Not responded".

        self $setClientConnectedStatus.
    ]

    #method $onServerRefuse
    [
        self $setDisconnectedStatus.
    ]

    #method $onAccept : aSocket
    [
        gui'prop'Items::SecondPlayer += "Not responded".
        gui'prop'Items::ThirdPlayer += "Not responded".
        gui'prop'Items::FourthPlayer += "Not responded".

        prop'Content::gui'prop'Tag::aSocket set:(gui'prop'Items::SecondPlayer Count - 1).

        prop'Writer::aSocket << net'AcceptCommand::(basic'LiteralType of:txtUserName).
    ]

    #method $onClientRead : aSocket
    [
        #var aCommand := prop'Reader::aSocket >> net'CommandFactory.

        #if aCommand 
            == net'AcceptID ?
            [ 
                prop'Writer::aSocket << self $replyOnAccept:aCommand.
            ]
            | == net'PlayerAcceptedID ?
            [
                self $onPlayerAccept:aCommand.
            ]
            | == net'PrepareGameID ?
            [
                players'Client new:thePlayer.
    
                aCommand proceed:thePlayer.
    
                self $startGameAsClient. 
            ].
        ]

    #method $onServerRead : aSocket
    [
        #var aCommand := prop'Reader::aSocket >> net'CommandFactory.

        #if(aCommand == net'AcceptReplyID)?
            [ self $acceptReply:{ Position = gui'prop'Tag::aSocket. prop'$getContent=aCommand. }. ].
    ]

    #method $replyOnAccept : aCommand
    [
        #var aPlayer := basic'String.

        aCommand >> aPlayer.
        gui'prop'Caption::HostName << aPlayer.

        ^ net'AcceptReplyCommand::(basic'LiteralType of:txtUserName).
    ]

    #method $onPlayerAccept : aCommand
    [
        #var aClientID := basic'Integer.
        #var aUser := basic'LiteralType of:txtUserName.
        
        aCommand >> aClientID.

        thePlayer := players'Human::
        { 
            config'$getFactoryCollection = 'players'ClientExtensionFactories.
            players'$getID = aClientID.
            players'$getName=aUser. 
            players'$getSocket = theParent $getClientSocket.
            players'$getForm = theParent.
        }.
    ]

    #method $acceptReply : anAcceptInfo
    [
        #var aPlayer := basic'String.
        prop'ContentValue::anAcceptInfo >> aPlayer.

        (prop'Indexer::gui'prop'Items::SecondPlayer << anAcceptInfo) delete:{ Count = 1. } insert:aPlayer.
        (prop'Indexer::gui'prop'Items::ThirdPlayer << anAcceptInfo) delete:{ Count = 1. } insert:aPlayer.
        (prop'Indexer::gui'prop'Items::FourthPlayer << anAcceptInfo) delete:{ Count = 1. } insert:aPlayer.
    ]

    #method $validate
    [
        #var aCheck := { 
            $validate : aCombo
            [
                #if (gui'prop'Visible::aCombo)!
                    [ ^ basic'True. ].

                #var aText := gui'prop'SelectedItem::aCombo.

                ^ ((aText Length != 0) and:("Not responded" != aText)).
            ]
        }.

        basic'True if:(aCheck $validate:SecondPlayer).
        basic'True if:(aCheck $validate:ThirdPlayer).
        basic'True if:(aCheck $validate:FourthPlayer).
    ]

    #method $acceptPlayers
    [
        #var aPlayerNumber := 2 + basic'IntegerType of:GameVariant.
        #var aRulebook := 'RuleBook::aPlayerNumber.

        self $validate.

        //engine'Master new:#annex('master'Events){ engine'$Rulebook=aPlayerNumber->upndown'RuleBook. }->engine'GameMaster.
        engine'Master new:engine'GameMaster::#annex(aRulebook)
        {
            config'$getFactoryCollection = 'master'Factories.
        }.

        engine'Players::engine'Master += thePlayer.

        #var NewClientPlayer := 
        { 
            create : aCombo
            [
                #var aName := basic'LiteralType of:gui'prop'SelectedItem::aCombo.
                #var aSocket := theParent $getRemoteSocket:(basic'IntNumberType of:gui'prop'SelectedIndex::aCombo).
                #var anID := (theCounter += 1) clone.

                prop'Content::gui'prop'Tag::aSocket set:anID.

                prop'Writer::aSocket << net'PlayerAcceptedCommand::anID.

                #var aPlayer := players'Base::
                { 
                    players'$getID = anID.
                    players'$getName=aName. 
                    config'$getFactoryCollection = 'players'RemoteExtensionFactories.
                    players'$getSocket = aSocket.
                    players'$getForm = theParent.
                }.

                ^ aPlayer.
            ]
        }.
        #if aPlayerNumber
        ==2 ?
        [
            engine'Players::engine'Master += NewClientPlayer create:SecondPlayer.
        ]
        | ==3 ?
        [
            engine'Players::engine'Master 
                += NewClientPlayer create:SecondPlayer
                += NewClientPlayer create:ThirdPlayer.
        ]
        | ==4 ?
        [
            engine'Players::engine'Master 
                += NewClientPlayer create:SecondPlayer
                += NewClientPlayer create:ThirdPlayer
                += NewClientPlayer create:FourthPlayer.
        ].

        gui'prop'Enabled::cmdLoad << basic'True.
        gui'prop'Enabled::cmdStart << basic'True.
        gui'prop'Visible::cmdDeny << basic'True.
        gui'prop'Visible::cmdAccept << basic'False.
        gui'prop'Enabled::cmdSearch << basic'False.

        gui'prop'Enabled::rbgConnectType << basic'False.
        gui'prop'Enabled::GameVariant << basic'False.

        gui'prop'Enabled::SecondPlayer << basic'False.
        gui'prop'Enabled::ThirdPlayer << basic'False.
        gui'prop'Enabled::FourthPlayer << basic'False.
        gui'prop'Enabled::txtUserName << basic'False.
        gui'prop'Enabled::txtPassword << basic'False.
    ]

    #method $denyPlayers
    [
        gui'prop'Enabled::cmdLoad << basic'False.
        gui'prop'Enabled::cmdStart << basic'False.
        gui'prop'Visible::cmdDeny << basic'False.
        gui'prop'Visible::cmdAccept << basic'True.
        gui'prop'Enabled::cmdSearch << basic'True.

        gui'prop'Enabled::rbgConnectType << basic'True.
        gui'prop'Enabled::GameVariant << basic'True.

        gui'prop'Enabled::SecondPlayer << basic'True.
        gui'prop'Enabled::ThirdPlayer << basic'True.
        gui'prop'Enabled::FourthPlayer << basic'True.
        gui'prop'Enabled::txtUserName << basic'True.
        gui'prop'Enabled::txtPassword << basic'True.

        //theParent::$closeServer.
    ]

    #method $startGameAsClient
    [
        gui'prop'DialogResult::self << ClientGame.
    ]

    #method $startGame
    [
        #var aResult := gui'InputBox::self open.
        #if(aResult)?
        [            
            gui'prop'DialogResult::self << ServerGame.
        ].
    ]
}

// --- TricksDlg ---

#class TricksDlg (forms'Dialog)
{
    #field theLabel1.
    #field theTrickNumber.
    #field cmOK.
    #field theUpDown.

    #role Modal
    {
        #method controls'$exitWithResult : aValue
        [
            self $onSetResult:aValue.

            gui'prop'Visible::self << basic'false.

            #shift.
    
            theResult := aValue.
        ]
    }

    #method forms'$initialize
    [
        theLabel1 := controls'StaticLabel::self.
        theTrickNumber := controls'Edit::self.
        theUpDown := controls'UpDown::self.
        cmOK := controls'Button::self.

        events'Handlers::cmOK +=
            { controls'$OnClick = self $onClick. } .

        gui'prop'Caption::self << "Ordering".
        gui'prop'Location::self << graph'Point{ X=199. Y=186. }.
        gui'prop'Size::self << graph'Size{ Width=364. Height=85. }.

        gui'prop'Location::theLabel1 << graph'Point{ X=12. Y=8. }.
        gui'prop'Size::theLabel1 << graph'Size{ Width=160. Height=34. }.
        gui'prop'Caption::theLabel1 << "Choose number of cards you are going to take".
        gui'prop'Visible::theLabel1 << basic'True.

        gui'prop'Location::theTrickNumber << graph'Point{ X=192. Y=13. }.
        gui'prop'Size::theTrickNumber << graph'Size{ Width=36. Height=24. }.
        theTrickNumber << "0".
        gui'prop'Visible::theTrickNumber << basic'True.
        
        gui'prop'Location::cmOK << graph'Point{ X=274. Y=13. }.
        gui'prop'Size::cmOK << graph'Size{ Width=81. Height=25. }.
        gui'prop'Caption::cmOK << "OK".
        gui'prop'Visible::cmOK << basic'True.

        gui'prop'Location::theUpDown << graph'Point{ X=228. Y=13. }.
        gui'prop'Size::theUpDown << graph'Size{ Width=19. Height=24. }.
        gui'prop'Visible::theUpDown << basic'True.            
        gui'prop'Range::theUpDown << { from = 0. till = self $getCardsNumber. }.

        super $invoke.
    ]

    #method $onClick
    [
        #var aRetVal := basic'IntNumberType of:basic'Variant::(basic'LiteralType of:theTrickNumber).

        #if(gui'prop'DialogResult::self) << aRetVal
            | [ ]. // TODO: show message
    ]
}

// --- Playground ---

#class Playground (forms'SDIDialog)
{
    #field theConnector.

    #field theServerSocket.
    #field theClientSocket.

    #field theStatusbar.
    #field lblPlayer1.
    #field lblPlayer2.
    #field lblPlayer3.
    #field lblPlayer4.

    #field thePaintbox.
    #field theImageList.    

    #field theLabelMap.
    #field theHand.
    #field theCardsnumber.
    #field theTrump.
    #field theChosen.
    #field theTable.
    #field theChosenCard.
    
    #field theImageSize.
    #field theHandRect.

    #role Starting
    {
        #method $getListener = theConnector.

        #method $start : aResult
        [
            #shift.

            self $start:aResult.
        ]
    }

    #role CurrentTurn
    {
        #method $waitingForTurn []

        #method players'$getCard = theChosenCard.

        #method $selectCard : aCard
        [
            theChosenCard := aCard.
            theChosen << aCard.

            theChosen := nil.
            #shift.
        ]
    }

    #method $getListener = self.

    #method new
    [
        theHand := gui'Hand::18.
        theTrump := basic'Integer << 0.
        theCardsnumber := basic'Integer.

        super new.
    ]

    #method $loadImageList
    [
        theImageList += "..\img\diamond_6.bmp".
        theImageList += "..\img\diamond_7.bmp".
        theImageList += "..\img\diamond_8.bmp".
        theImageList += "..\img\diamond_9.bmp".
        theImageList += "..\img\diamond_10.bmp".
        theImageList += "..\img\diamond_jack.bmp".
        theImageList += "..\img\diamond_queen.bmp".
        theImageList += "..\img\diamond_king.bmp".
        theImageList += "..\img\diamond_ace.bmp".

        theImageList += "..\img\club_6.bmp".
        theImageList += "..\img\club_7.bmp".
        theImageList += "..\img\club_8.bmp".
        theImageList += "..\img\club_9.bmp".
        theImageList += "..\img\club_10.bmp".
        theImageList += "..\img\club_jack.bmp".
        theImageList += "..\img\club_queen.bmp".
        theImageList += "..\img\club_king.bmp".
        theImageList += "..\img\club_ace.bmp".

        theImageList += "..\img\heart_6.bmp".
        theImageList += "..\img\heart_7.bmp".
        theImageList += "..\img\heart_8.bmp".
        theImageList += "..\img\heart_9.bmp".
        theImageList += "..\img\heart_10.bmp".
        theImageList += "..\img\heart_jack.bmp".
        theImageList += "..\img\heart_queen.bmp".
        theImageList += "..\img\heart_king.bmp".
        theImageList += "..\img\heart_ace.bmp".

        theImageList += "..\img\spade_6.bmp".
        theImageList += "..\img\spade_7.bmp".
        theImageList += "..\img\spade_8.bmp".
        theImageList += "..\img\spade_9.bmp".
        theImageList += "..\img\spade_10.bmp".
        theImageList += "..\img\spade_jack.bmp".
        theImageList += "..\img\spade_queen.bmp".
        theImageList += "..\img\spade_king.bmp".
        theImageList += "..\img\spade_ace.bmp".

        theImageSize := graph'SizeVar << gui'prop'Size::(theImageList@0).
        theHandRect := graph'Rectangle::
        {
            X = hndLeft.
            Y = hndTop.
            Width = hndShift * 18.
            Height = theImageSize Height.
        }.
    ]

    #method forms'$initialize
    [
        theServerSocket := sockets'ServerSocket::self.
        theClientSocket := sockets'ClientSocket::self.

        theStatusbar := controls'Statusbar::self.
        lblPlayer1 := controls'StaticLabel::self.
        lblPlayer2 := controls'StaticLabel::self.
        lblPlayer3 := controls'StaticLabel::self.
        lblPlayer4 := controls'StaticLabel::self.

        thePaintbox := controls'Paintbox::self.
        theImageList := graph'ImageList::self.

        self $loadImageList.

        gui'prop'Size::self << graph'Size{ Width=700. Height=520.}.
        gui'prop'Location::self << graph'Point{ X=50. Y=50.}.
        gui'prop'Visible::self << basic'true.
        gui'prop'Caption::self << "Up'N'Down".

        gui'prop'Size::thePaintbox << graph'Size{ Width=700. Height=500.}.
        gui'prop'Location::thePaintbox << graph'Point{ X=0. Y=0.}.
        gui'prop'Visible::thePaintbox << basic'true.

        gui'prop'Location::theStatusbar << graph'Point{ X=0. Y=467. }.
        gui'prop'Size::theStatusbar << graph'Size{ Width=700. Height=20. }.
        theStatusbar create:(104,284,464,690).
        gui'prop'Items::theStatusbar@0 << "Up'N'Down".

        gui'prop'Visible::theStatusbar << basic'true.

        gui'prop'Location::lblPlayer1 << graph'Point{ X=182. Y=433. }.
        gui'prop'Size::lblPlayer1 << graph'Size{ Width=200. Height=15. }.

        events'Handlers::self +=
        {
            forms'$onOpen = self $onOpen.
        }.
        
        events'Handlers::theClientSocket += 
        { 
            sockets'$OnSocketDisconnect = self $getListener $onDisconnect.  

            sockets'$OnConnectRefuse = self $getListener $onServerRefuse.

            sockets'$OnSocketConnect = self $getListener $onConnect.

            sockets'$onRead : aSocket= self $getListener $onClientRead:aSocket.
        }.

        events'Handlers::theServerSocket += 
        { 
            sockets'$OnServerAccept : aSocket = self $getListener $onAccept:aSocket.

            sockets'$onRead : aSocket = self $getListener $onServerRead:aSocket.
        }.

        events'Handlers::thePaintbox +=
            { controls'$onRender : aHandle = self $onPaint:aHandle. controls'$onLButton : aCoord = self $onMouseDown:aCoord. }.

        super $invoke.
    ]

    #method $onOpen
    [
        theConnector := gui'Connector::self.

        #shift Starting.

        #var aResult := theConnector open:self.
        #if(aResult)?
            [] | [ 'program exit. ].

        theConnector := basic'nil. // !! check if window is destroyed and removed from referencepool

        self $start:aResult.
    ]

    #method $start : aResult
    [
		#if aResult $isServerGame
			[ engine'Master start. ].
    ]

    #method $getServerSocket = theServerSocket.

    #method $getClientSocket = theClientSocket.

    #method $getRemoteSocket : anIndex = prop'ContentValue::(theServerSocket@anIndex).

    #method $startSearch
    [
        sockets'Port::theServerSocket << 401h.

        theServerSocket open.
    ]

    #method $connectToServer : anIPAddress
    [        
        sockets'Port::theClientSocket << 401h.
        sockets'IPAddress::theClientSocket << anIPAddress.

        theClientSocket open.
    ]

    #method $onClientRead : aSocket
    [
        #var aCommand := prop'Reader::aSocket >> net'CommandFactory.

        aCommand proceed:players'Client.
    ]    
        
    #method $onServerRead : aSocket
    [
        #var aCommand := prop'Reader::aSocket >> net'CommandFactory.
        #var anIndex := gui'prop'Tag::aSocket.
        #var aPlayer := ctrl'SearchAgent::engine'GamePlayers proceed:{ == aValue = (prop'ContentKey::aValue == anIndex). }.

        aCommand proceed:aPlayer.
    ]

    #method $onPaint : aCanvas
    [   
        #var aDrawing := graph'Drawing::aCanvas.

        // Drawing hand
        prop'IndexEnumerator::theHand run: aCard =>
        [
            #if(aCard != 0)?
            [
                aDrawing << graph'Point { X=hndLeft + (prop'Index::aCard * hndShift). Y=hndTop. }.

                aDrawing proceed:(theImageList@(aCard - 1)).
            ].
        ].

        // Drawing trump card
        #if(0 != theTrump)?
        [
            aDrawing << graph'Point { X=trmLeft. Y=trmTop. }.

            aDrawing proceed:(theImageList@(theTrump - 1)).
        ].

        // Drawing table cards
        prop'IndexEnumerator::theTable run: aCard =>
        [
            #if(aCard != 0)?
            [
                aDrawing << tblCoords@(prop'Index::aCard).

                aDrawing proceed:(theImageList@(aCard - 1)).
            ].
        ].
    ]

    #method $onMouseDown: aCoord
    [
        #if(theHandRect isValid:aCoord)?
        [
            #var aWidth := theImageSize Width.
            #var anX := aCoord X - hndLeft.
            
            prop'IndexEnumerator::theHand run: aCard =>
            [
                #if(aCard != 0)?
                [
                    #var aCardX := prop'Index::aCard * hndShift.
                    #if((aCardX < anX)and:(aCardX + aWidth > anX))?
                    [
                        self $selectCard:aCard.

                        theHand -= aCard.
                        self refresh.
                    ].
                ].
            ].
        ].
    ]

    #method $setPlayerNumber : aPlayerNumber
    [
        theLabelMap := basic'ArrayType create:{ Length = aPlayerNumber. }.

        gui'prop'Visible::lblPlayer1 << basic'true.

        #if aPlayerNumber 
            == 2 ?
            [
                gui'prop'Location::lblPlayer2 << graph'Point{ X=201. Y=8. }.
                gui'prop'Size::lblPlayer2 << graph'Size{ Width=200. Height=15. }.
                gui'prop'Visible::lblPlayer2 << basic'true.
            ]
            | == 3 ?
            [
                gui'prop'Location::lblPlayer2 << graph'Point{ X=45. Y=18. }.
                gui'prop'Size::lblPlayer2 << graph'Size{ Width=200. Height=15. }.
                gui'prop'Visible::lblPlayer2 << basic'true.
        
                gui'prop'Location::lblPlayer3 << graph'Point{ X=341. Y=16. }.
                gui'prop'Size::lblPlayer3 << graph'Size{ Width=200. Height=15. }.
                gui'prop'Visible::lblPlayer3 << basic'true.
            ]
            | == 4 ?
            [
                gui'prop'Location::lblPlayer2 << graph'Point{ X=13. Y=32. }.
                gui'prop'Size::lblPlayer2 << graph'Size{ Width=200. Height=15. }.
                gui'prop'Visible::lblPlayer2 << basic'true.

                gui'prop'Location::lblPlayer3 << graph'Point{ X=201. Y=8. }.
                gui'prop'Size::lblPlayer3 << graph'Size{ Width=200. Height=15. }.
                gui'prop'Visible::lblPlayer3 << basic'true.
        
                gui'prop'Location::lblPlayer4 << graph'Point{ X=357. Y=32. }.
                gui'prop'Size::lblPlayer4 << graph'Size{ Width=200. Height=15. }.
                gui'prop'Visible::lblPlayer4 << basic'true.
            ].
    
        theTable := basic'ArrayType create:{ Length = aPlayerNumber. Create = basic'Integer << 0. }.
    ]

    #method $findPlayerInfo : aPlayer
    [
        ^ ctrl'SearchAgent::theLabelMap proceed:ctrl'KeyComparator::aPlayer.
    ]

    #method players'$onPrepare : Players
    [
        self $setPlayerNumber:(Players Count).

        #var Labels := (lblPlayer1,lblPlayer2,lblPlayer3,lblPlayer4).

        prop'IndexEnumerator::Players run: aPlayer =>
        [
            #var anID := prop'ContentKey::aPlayer.
            #var aName := ext'text'PrintValue::aPlayer.
            #var anIndex := basic'IntNumber::prop'Index::aPlayer.
            #var aLabel := prop'ContentValue::(Labels@anIndex).

            prop'Content::(theLabelMap@anIndex) set:
            {
                prop'$getKey     = anID. 
                $getLabel = aLabel. 
                $getIndex = anIndex.
                $getName = aName. 
            }.

            gui'prop'Caption::aLabel << ext'text'PrintValue::aPlayer.
        ].
    ]

    #method players'$onNewLeadingRound : aRound
    [
        theHand clear.
        theTrump << 0.
        theCardsnumber << game'Cardsnumber::aRound.

        gui'prop'Items::theStatusbar @0 << ext'text'PrintValue::aRound.             

        gui'prop'Items::theStatusbar @1 << ("you are distributor").
    ]

    #method players'$onNewRound : aRound
    [
        #var anInfo := self $findPlayerInfo:game'Distributor::aRound.

        theHand clear.
        theTrump << 0.
        theCardsnumber << game'Cardsnumber::aRound.

        // clear the table
        prop'Enumerator::theTable run: anItem => ( anItem << 0 ).

        gui'prop'Items::theStatusbar @0 << ext'text'PrintValue::aRound.
        
        gui'prop'Items::theStatusbar @1 << (basic'LiteralType of:(anInfo $getName) + " is distributor").
    ]

    #method players'$distributeCard : aCard
    [
        theHand += aCard.

        self refresh.
    ]

    #method players'$selectTrump : aCard
    [
        theTrump << aCard.

        self refresh.
    ]

    #method players'$makeOrder : anOrder
    [
        /*#var aForm := gui'TricksDlg
        { 
            $getCardsnumber = theCardsnumber.

            $onSetResult : aResult 
            [ 
                anOrder << aResult. 
            ] 
        }::self.

        aForm open.*/
    ]

    #method players'$onOrdered : anOrdered
    [
        #var anInfo := self $findPlayerInfo:anOrdered.

        gui'prop'Caption::(anInfo $getLabel) << (basic'LiteralType of:(anInfo $getName) + " (0 / " + basic'Variant::anOrdered + ")").
    ]

    #method players'$onBeforeMove : aPlayer
    [
        #var anInfo := self $findPlayerInfo:aPlayer.

        (gui'prop'Items::theStatusbar @2) << (basic'LiteralType of:(anInfo $getName) + " turn...").
    ]

    #method players'$onBeforeYourMove
    [
        (gui'prop'Items::theStatusbar @2) << "Your turn...".        
    ]

    #method players'$onShowCard : aCard
    [
        #var anInfo := self $findPlayerInfo:aCard.

        theTable@(anInfo $getIndex) << aCard.

        self refresh.
    ]

    #method players'$getPlayerCard : aPlayer
    [
        #var anInfo := self $findPlayerInfo:aPlayer.

        ^ basic'IntNumberType of:(theTable@(anInfo $getIndex)).
    ]
    
    #method players'$getTrump = theTrump.

    #method players'$putCard : aValidator
    [
        theChosen := aValidator.

        #shift CurrentTurn.

        #loop $self $waitingForTurn
        [
            'program continue.
        ].
    ]
}

// --- InputBox ---

#class InputBox (forms'Dialog)
{
    #field GameName.
    #field cmOK.
    #field cmCancel.

    #method forms'$initialize
    [
        // --- Create controls ---
		GameName := controls'Edit::self.
        cmOK := controls'Button::self.		
        cmCancel := controls'Button::self.

        gui'prop'Caption::self << "Enter the game name".
        gui'prop'Location::self << graph'Point{ X=281. Y=220. }.
        gui'prop'Size::self << graph'Size{ Width=270. Height=100. }.

        gui'prop'Location::GameName << graph'Point{ X=8. Y=16. }.
        gui'prop'Size::GameName << graph'Size{ Width=250. Height=24. }.
        gui'prop'Visible::GameName << basic'True.

        gui'prop'Location::cmOK << graph'Point{ X=48. Y=48. }.
        gui'prop'Size::cmOK << graph'Size{ Width=75. Height=25. }.
        gui'prop'Caption::cmOK << "OK".
        gui'prop'Visible::cmOK << basic'True.
        gui'prop'Enabled::cmOK << basic'False.

        gui'prop'Location::cmCancel << graph'Point{ X=152. Y=48. }.
        gui'prop'Size::cmCancel << graph'Size{ Width=75. Height=25. }.
        gui'prop'Caption::cmCancel << "Cancel".
        gui'prop'Visible::cmCancel << basic'True.

        events'Handlers::cmOK += 
            { controls'$onClick = gui'prop'DialogResult::self << basic'true. }.

        events'Handlers::cmCancel += 
            { controls'$onClick = gui'prop'DialogResult::self << basic'false. }.

        events'Handlers::GameName += 
            { controls'$onChange = self $onGameNameChange. }.
    
        super $invoke.
    ]

    #method $onGameNameChange
    [
        gui'prop'Enabled::cmOK << (basic'LiteralType of:GameName Length != 0).
    ]
}
