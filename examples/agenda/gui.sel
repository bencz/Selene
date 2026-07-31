#define propts'*    = std'properties'*.
#define basic'*    = std'basic'*.
#define forms'*    = gui'forms'*.
#define prop'*     = gui'controls'properties'*.
#define graph'*    = gui'graphics'*.
#define controls'* = gui'controls'*.
#define events'*   = sys'events'*.

#class window (forms'SDIDialog)
{
    // All Text Box's
    #field Name.
    #field SurName.
    #field OtName.
    #field Email.
    #field Age.
    #field PhoneNumber.
    
    // All Labels
    #field LabelName.
    #field LabelSurName.
    #field LabelOtName.
    #field LabelEmail.
    #field LabelAge.
    #field LabelPhoneNumber.
    #field LabelAbout.
    #field LabelDel.

    // All Buttons
    #field BtmExit.
    #field BtmSave.
    #field BtmDelTxt.
    #field BtmInfo.

    #method new
    [ super new. ]
    
    #method forms'$initialize
    [
        super $invoke.

        // --- Create controls the Labels ---
        LabelName := controls'StaticLabel::self.
        LabelSurName := controls'StaticLabel::self.
        LabelOtName := controls'StaticLabel::self.
        LabelEmail := controls'StaticLabel::self.
        LabelAge := controls'StaticLabel::self.
        LabelPhoneNumber := controls'StaticLabel::self.
        LabelAbout := controls'StaticLabel::self.
        LabelDel := controls'StaticLabel::self.

        // --- Create the Text Box controls ---
        Name := controls'Edit::self.
        SurName := controls'Edit::self.
        OtName := controls'Edit::self.
        Email := controls'Edit::self.
        Age := controls'Edit::self.
        PhoneNumber := controls'Edit::self.
        
        // --- Create controls of the Buttons ---
        BtmExit := controls'Button::self.
        BtmSave := controls'Button::self.
        BtmDelTxt := controls'Button::self.
        BtmInfo := controls'Button::self.

        // -------------------------------------------------------------------------------
        
        // Form
        prop'Caption::self << "Writer Account".
        prop'Size::self << graph'Size{ Width=500. Height=235. }.
        prop'Location::self << graph'Point{ X=350. Y=250.}.
        
        // ------------------------------------ Labels ------------------------------------
        // LabelName
        prop'Caption::Labelname << "Name:".
        prop'Size::Labelname << graph'Size{ Width=41. Height=20. }.
        prop'Location::Labelname << graph'Point{ X=12. Y=15. }.
        prop'Visible::Labelname << basic'true.

        // LabelSurName
        prop'Caption::LabelSurName << "Your Last Name:".
        prop'Size::LabelSurName << graph'Size{ Width=109. Height=20. }.
        prop'Location::LabelSurName << graph'Point{ X=12. Y=40. }.
        prop'Visible::LabelSurName << basic'true.

        // LabelOtName
        prop'Caption::LabelOtName << "Other Names:".
        prop'Size::LabelOtName << graph'Size{ Width=89. Height=20. }.
        prop'Location::LabelOtName << graph'Point{ X=12. Y=65. }.
        prop'Visible::LabelOtName << basic'true.

        // LabelEmail
        prop'Caption::LabelEmail << "Email:".
        prop'Size::LabelEmail << graph'Size{ Width=40. Height=20. }.
        prop'Location::LabelEmail << graph'Point{ X=12. Y=90. }.
        prop'Visible::LabelEmail << basic'true.

        // LabelAge
        prop'Caption::LabelAge << "Age:".
        prop'Size::LabelAge << graph'Size{ Width=27. Height=20. }.
        prop'Location::LabelAge << graph'Point{ X=12. Y=115. }.
        prop'Visible::LabelAge << basic'true.

        // LabelPhoneNumber
        prop'Caption::LabelPhoneNumber << "Phone Number:".
        prop'Size::LabelPhoneNumber << graph'Size{ Width=100. Height=20. }.
        prop'Location::LabelPhoneNumber << graph'Point{ X=12. Y=140. }.
        prop'Visible::LabelPhoneNumber << basic'true.

        // LabelAbout
        prop'Size::LabelAbout << graph'Size{ Width=250. Height=35. }.
        prop'Location::LabelAbout << graph'Point{ X=12. Y=165. }.
        prop'Visible::LabelAbout << basic'true.
        
        // LabelDel
        prop'Size::LabelDel << graph'Size{ Width=250. Height=35. }.
        prop'Location::LabelDel << graph'Point{ X=300. Y=165. }.
        prop'Visible::LabelDel << basic'true.

        // ------------------------------------ Text Box's ------------------------------------
        // Name
        Name << "Your name".
        prop'Size::Name << graph'Size{ Width=200. Height=25. }.
        prop'Location::Name << graph'Point{ X=125. Y=10. }.
        prop'Visible::Name << basic'true.

        // SurName
        SurName << "Your Last Name".
        prop'Size::SurName << graph'Size{ Width=200. Height=25. }.
        prop'Location::SurName << graph'Point{ X=125. Y=35. }.
        prop'Visible::SurName << basic'true.

        // OtName
        OtName << "Other Name".
        prop'Size::OtName << graph'Size{ Width=200. Height=25. }.
        prop'Location::OtName << graph'Point{ X=125. Y=60. }.
        prop'Visible::OtName << basic'true.

        // Email
        Email << "Email".
        prop'Size::Email << graph'Size{ Width=200. Height=25. }.
        prop'Location::Email << graph'Point{ X=125. Y=85. }.
        prop'Visible::Email << basic'true.

        // Age
        Age << "Age".
        prop'Size::Age << graph'Size{ Width=200. Height=25. }.
        prop'Location::Age << graph'Point{ X=125. Y=110. }.
        prop'Visible::Age << basic'true.

        // PhoneNumber
        PhoneNumber << "Phone Number".
        prop'Size::PhoneNumber << graph'Size{ Width=200. Height=25. }.
        prop'Location::PhoneNumber << graph'Point{ X=125. Y=135. }.
        prop'Visible::PhoneNumber << basic'true.

        // ------------------------------------ Buttons ------------------------------------
        // BtmSave
        prop'Caption::BtmSave << "Save".
        prop'Size::BtmSave << graph'Size{ Width=65. Height=20. }.
        prop'Location::BtmSave << graph'Point{ X=410. Y=20.}.
        prop'Visible::BtmSave << basic'true.

        // BtmDelTxt
        prop'Caption::BtmDelTxt << "Del TXT".
        prop'Size::BtmDelTxt << graph'Size{ Width=65. Height=20. }.
        prop'Location::BtmDelTxt << graph'Point{ X=410. Y=50.}.
        prop'Visible::BtmDelTxt << basic'true.

        // BtmInfo
        prop'Caption::BtmInfo << "About".
        prop'Size::BtmInfo << graph'Size{ Width=65. Height=20. }.
        prop'Location::BtmInfo << graph'Point{ X=410. Y=80.}.
        prop'Visible::BtmInfo << basic'true.

        // BtmExit
        prop'Caption::BtmExit << "Exit".
        prop'Size::BtmExit << graph'Size{ Width=65. Height=20. }.
        prop'Location::BtmExit << graph'Point{ X=410. Y=110.}.
        prop'Visible::BtmExit << basic'true.

        // ------------------------------------ Handlers  ------------------------------------    
        // Exit
        events'Handlers::BtmExit +=
        {
            controls'$onClick
            [
                'program exit.
            ]
        }.
        
        // Save
        events'Handlers::BtmSave +=
        {
            controls'$onClick
            [
                #var File := win32'io'TextFileAppend::"files.txt".
                #var Writer := propts'TextWriter::File.
                Writer << "//-------------------------------------------------------------------------------%n". 
                Writer << "%r%nName: " << Name << " " << SurName << " " << OtName.
                Writer << "%r%nEmail: " << Email.
                Writer << "%r%nAge: " << Age.
                Writer << "%r%nPhoneNumber: " << PhoneNumber.
                Writer << "%r%n//-------------------------------------------------------------------------------%n".

                File close.
            ]
        }.
        
        // Del TXT
        events'Handlers::BtmDelTxt +=
        {
            controls'$onClick
            [
                prop'Caption::LabelDel << "Function Del TXT not created%n(Temporary)".
            ]
        }.

        // About
        events'Handlers::BtmInfo +=
        {
            controls'$onClick
            [
                prop'Caption::LabelAbout << "Created by Alexandre Bencz.%nThanks Alex Rakov, for support.".
            ]
        }.
    ]
}