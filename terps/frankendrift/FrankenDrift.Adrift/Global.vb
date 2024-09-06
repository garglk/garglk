Imports System.Drawing
Imports FrankenDrift.Glue
Imports FrankenDrift.Glue.Util

Public Module SharedModule
    Public Property Adventure As clsAdventure
    Public Glue As UIGlue
    Public fRunner As frmRunner
    Public UserSession As RunnerSession

    Public Blorb As clsBlorb
    Public iLoading As Integer
    Public dVersion As Double = 5.0
    Public Const ANYOBJECT As String = "AnyObject"
    Public Const ANYCHARACTER As String = "AnyCharacter"
    Public Const ANYDIRECTION As String = "AnyDirection"
    Public Const NOOBJECT As String = "NoObject"
    Public Const NOCHARACTER As String = "NoCharacter"
    Public Const ALLROOMS As String = "AllLocations"
    Public Const NOROOMS As String = "NoLocations"
    Public Const THEFLOOR As String = "TheFloor"
    Public Const THEPLAYER As String = "%Player%"
    Public Const CHARACTERPROPERNAME As String = "CharacterProperName"
    Public Const PLAYERLOCATION As String = "PlayerLocation"
    Public Const HIDDEN As String = "Hidden"

    Public Const DEFAULT_BACKGROUNDCOLOUR As Integer = -16777216
    Public Const DEFAULT_INPUTCOLOUR As Integer = -3005145
    Public Const DEFAULT_OUTPUTCOLOUR As Integer = -15096438
    Public Const DEFAULT_LINKCOLOUR As Integer = -11806788

    ' Mandatory properties
    Public Const SHORTLOCATIONDESCRIPTION As String = "ShortLocationDescription"
    Public Const LONGLOCATIONDESCRIPTION As String = "LongLocationDescription"
    Public Const OBJECTARTICLE As String = "_ObjectArticle"
    Public Const OBJECTPREFIX As String = "_ObjectPrefix"
    Public Const OBJECTNOUN As String = "_ObjectNoun"


    Public Const sLOCATION As String = "Location"
    Public Const sOBJECT As String = "Object"
    Public Const sTASK As String = "Task"
    Public Const sEVENT As String = "Event"
    Public Const sCHARACTER As String = "Character"
    Public Const sGROUP As String = "Group"
    Public Const sVARIABLE As String = "Variable"
    Public Const sPROPERTY As String = "Property"
    Public Const sHINT As String = "Hint"
    Public Const sALR As String = "Text Override"
    Public Const sGENERAL As String = "General"
    Public Const sSELECTED As String = "<Selected>"
    Public Const sUNSELECTED As String = "<Unselected>"

    Public iImageSizeMode As Integer

    Public Enum PerspectiveEnum
        None = 0
        FirstPerson = 1     ' I/Me/Myself
        SecondPerson = 2    ' You/Yourself
        ThirdPerson = 3     ' He/She/Him/Her
        ' It
        ' We
        ' They
    End Enum

    Public Enum ReferencesType
        [Object]
        Character
        Number
        Text
        Direction
        Location
        Item
    End Enum

    Public Enum ArticleTypeEnum
        Definite ' The
        Indefinite ' A
        None
    End Enum

#Region "Microsoft.VisualBasic compatability"

    Public Function Asc(ByVal c As Char) As Integer
        Return Convert.ToInt32(c)
    End Function
    Public Function Asc(ByVal s As String) As Integer
        Return System.Text.Encoding.Default.GetBytes(s(0))(0)
    End Function

#End Region

    Public ReadOnly Property BinPath(Optional ByVal bSeparatorCharacter As Boolean = False) As String
        Get
            Return Application.StartupPath & If(bSeparatorCharacter, IO.Path.DirectorySeparatorChar, "").ToString
        End Get
    End Property
    Public ReadOnly Property DataPath(Optional ByVal bSeparatorCharacter As Boolean = False) As String
        Get
            Dim sPath As String
            sPath = Application.LocalUserAppDataPath
            If Not IO.Directory.Exists(sPath) Then IO.Directory.CreateDirectory(sPath)
            Return sPath & If(bSeparatorCharacter, IO.Path.DirectorySeparatorChar, "").ToString
        End Get
    End Property

    Private sWarnings As New List(Of String)
    Public Sub WarnOnce(ByVal sMessage As String)

        If Not sWarnings.Contains(sMessage) Then
            ErrMsg(sMessage)
            sWarnings.Add(sMessage)
        End If

    End Sub

    Private r As Random
    Public Function Random(ByVal iMax As Integer) As Integer
        If r Is Nothing Then r = New Random(GetNextSeed)
        Return r.Next(iMax + 1)
    End Function
    Public Function Random(ByVal iMin As Integer, ByVal iMax As Integer) As Integer
        If r Is Nothing Then r = New Random(GetNextSeed)
        If iMax < iMin Then
            Dim i As Integer = iMax
            iMax = iMin
            iMin = i
        End If
        Return r.Next(iMin, iMax + 1)
    End Function

    Private Function GetNextSeed() As Integer
        Static iLastSeed As Integer = 0

        Dim iNextSeed As Integer = CInt(Now.Ticks Mod Integer.MaxValue)
        While iNextSeed = iLastSeed
            iNextSeed -= Now.Millisecond
        End While
        iLastSeed = iNextSeed

        Return iNextSeed

    End Function


    Public Enum DirectionsEnum
        North = 0
        East = 1
        South = 2
        West = 3
        Up = 4
        Down = 5
        [In] = 6
        Out = 7
        NorthEast = 8
        SouthEast = 9
        SouthWest = 10
        NorthWest = 11
    End Enum

    Public Enum ItemEnum
        Location
        [Object]
        Task
        [Event]
        Character
        Group
        Variable
        [Property]
        Hint
        ALR
        General
    End Enum

    Public Sub GlobalStartup()
        Dim version As Version = New Version(Application.ProductVersion)
        dVersion = CDbl(version.Major & System.Globalization.CultureInfo.CurrentCulture.NumberFormat.NumberDecimalSeparator & Format(version.Minor, "000") & Format(version.Build, "000") & Format(version.Revision))
    End Sub

    Friend Function IsHex(ByVal sHex As String) As Boolean
        For i As Integer = 0 To sHex.Length - 1
            If "0123456789ABCDEF".IndexOf(sHex(i)) = -1 Then Return False
        Next
        Return True
    End Function

    Public Function ItemToString(ByVal item As ItemEnum, Optional ByVal bPlural As Boolean = False) As String
        ItemToString = ""

        Select Case item
            Case ItemEnum.Location
                ItemToString = sLOCATION
            Case ItemEnum.Object
                ItemToString = sOBJECT
            Case ItemEnum.Task
                ItemToString = sTASK
            Case ItemEnum.Event
                ItemToString = sEVENT
            Case ItemEnum.Character
                ItemToString = sCHARACTER
            Case ItemEnum.Group
                ItemToString = sGROUP
            Case ItemEnum.Variable
                ItemToString = sVARIABLE
            Case ItemEnum.Property
                ItemToString = sPROPERTY
            Case ItemEnum.Hint
                ItemToString = sHINT
            Case ItemEnum.ALR
                ItemToString = sALR
            Case ItemEnum.General
                ItemToString = sGENERAL
        End Select

        If bPlural Then
            Select Case item
                Case ItemEnum.Location, ItemEnum.Object, ItemEnum.Task, ItemEnum.Event, ItemEnum.Character, ItemEnum.Group, ItemEnum.Variable, ItemEnum.Hint, ItemEnum.ALR, ItemEnum.General
                    Return ItemToString & "s"
                Case ItemEnum.Property
                    Return "Properties"
            End Select
        End If
    End Function

    Public Sub ErrMsg(ByVal sMessage As String, Optional ByVal ex As System.Exception = Nothing)
        Glue.ErrMsg(sMessage, ex)
    End Sub

    Public Function CharacterCount(ByVal sText As String, ByVal cCharacter As Char) As Integer
        CharacterCount = 0
        For i As Integer = 0 To sText.Length - 1
            If sText.Substring(i, 1) = cCharacter Then CharacterCount += 1
        Next
    End Function


    Public Function GetLibraries() As String()

        Dim sLibraries As String = GetSetting("ADRIFT", "Generator", "Libraries")

        If sLibraries = "" Then
            ' Attempt to find the libarary in current directory
            Dim SL As String = "StandardLibrary.amf"

            For Each sDir As String In New String() {IO.Path.GetDirectoryName(SafeString(Application.ExecutablePath)), DataPath}
                If IO.File.Exists(sDir & IO.Path.DirectorySeparatorChar & SL) Then
                    sLibraries = sDir & IO.Path.DirectorySeparatorChar & SL
                    Exit For
                End If
            Next
        End If

        Return sLibraries.Split("|"c)

    End Function

    ' A replacement for VB.Left - Substring returns an error if you try to access part of the string that doesn't exist.
    Public Function sLeft(ByVal sString As String, ByVal iLength As Integer) As String
        If sString Is Nothing OrElse iLength < 0 Then Return ""
        If sString.Length < iLength Then
            If sString <> Left(sString, iLength) Then
                sString = sString
            End If
            Return sString
        Else
            If sString.Substring(0, Math.Max(iLength, 0)) <> Left(sString, iLength) Then
                sString = sString
            End If
            Return sString.Substring(0, Math.Max(iLength, 0))
        End If
    End Function

    ' A replacement for VB.Right
    Public Function sRight(ByVal sString As String, ByVal iLength As Integer) As String
        If sString Is Nothing OrElse iLength < 0 Then Return ""
        If iLength > sString.Length Then
            If sString <> Right(sString, iLength) Then
                sString = sString
            End If
            Return sString
        Else
            If sString.Substring(sString.Length - iLength) <> Right(sString, iLength) Then
                sString = sString
            End If
            Return sString.Substring(sString.Length - iLength)
        End If
    End Function

    ' A replacement for VB.Mid
    Public Function sMid(ByVal sString As String, ByVal iStart As Integer) As String
        If sString Is Nothing Then Return ""
        If iStart < 1 Then iStart = 1
        If iStart > sString.Length Then
            Return ""
        Else
            Return sString.Substring(iStart - 1)
        End If
    End Function

    Public Function sMid(ByVal sString As String, ByVal iStart As Integer, ByVal iLength As Integer) As String
        If sString Is Nothing Then Return ""
        If iLength < 0 Then Return ""
        If iStart < 1 Then iStart = 1
        If iStart > sString.Length Then
            Return ""
        ElseIf iLength > sString.Length OrElse iLength + iStart > sString.Length Then
            Return sString.Substring(iStart - 1)
        Else
            Return sString.Substring(iStart - 1, iLength)
        End If
    End Function


    Public Function sInstr(ByVal sText As String, ByVal sSearchFor As String) As Integer
        If InStr(sText, sSearchFor) <> sInstr(1, sText, sSearchFor) Then
            sText = sText
        End If
        Return sInstr(1, sText, sSearchFor)
    End Function
    Public Function sInstr(ByVal startIndex As Integer, ByVal sText As String, ByVal sSearchFor As String) As Integer
        If sText Is Nothing OrElse sText = "" Then Return 0 ' -1 
        If sText.IndexOf(sSearchFor, startIndex - 1) + 1 <> InStr(startIndex, sText, sSearchFor) Then
            sText = sText
        End If
        Return sText.IndexOf(sSearchFor, startIndex - 1) + 1
    End Function

    Public Sub Source2HTML(ByVal sSource As String, ByRef RichText As RichTextBox, ByVal bClearRTB As Boolean, Optional ByVal bDebug As Boolean = False, Optional ByRef sUnprocessedText As String = Nothing)
        Glue.OutputHTML(sSource)
    End Sub

    ' Escape any characters that are special in RE's
    Friend Function MakeTextRESafe(ByVal sText As String) As String
        sText = sText.Replace("+", "\+").Replace("*", "\*")

        Return sText
    End Function


    Friend Function invhex(ByVal hexcode As String) As Byte
        Dim n As Byte

        If Left(hexcode, 1) = "0" Then
            For n = 0 To 15
                If Hex(n) = UCase(Right(hexcode, 1)) Then Return n
            Next n
        End If
        For n = 16 To 254
            If Hex(n) = UCase(hexcode) Then Return n
        Next n
        If Hex(255) = UCase(hexcode) Then Return 255
        Return 0
    End Function

    Friend Function ColourLookup(ByVal sCode As String) As String
        sCode = sCode.Replace("""", "")

        ColourLookup = Nothing

        Select Case LCase(sCode)
            Case "black" : ColourLookup = "000000"
            Case "blue" : ColourLookup = "0000ff"
            Case "cyan", "turquoise", "aqua" : ColourLookup = "00ffff"
            Case "default"
            Case "gray" : ColourLookup = "808080"
            Case "green" : ColourLookup = "008000"
            Case "lime" : ColourLookup = "00ff00"
            Case "magenta", "fuchsia" : ColourLookup = "ff00ff"
            Case "maroon" : ColourLookup = "800000"
            Case "navy" : ColourLookup = "000080"
            Case "olive" : ColourLookup = "808000"
            Case "orange" : ColourLookup = "ff8000"
            Case "pink" : ColourLookup = "ff8888"
            Case "purple" : ColourLookup = "800080"
            Case "red" : ColourLookup = "ff0000"
            Case "silver" : ColourLookup = "c0c0c0"
            Case "teal" : ColourLookup = "008080"
            Case "white" : ColourLookup = "ffffff"
            Case "yellow" : ColourLookup = "ffff00"

            Case Else
                If Left(sCode, 1) = "#" Then sCode = Right(sCode, Len(sCode) - 1)
                If Len(sCode) > 6 Then sCode = "ffffff"
                While Len(sCode) < 6
                    sCode = sCode & "0"
                End While
                ColourLookup = sCode
        End Select
    End Function

    Public Function DirectionName(ByVal dir As DirectionsEnum) As String
        Dim sName As String = Adventure.sDirectionsRE(dir)
        If sName.Contains("/") Then sName = sName.Substring(0, sName.IndexOf("/"))
        Return sName
    End Function



    Public Function DirectionRE(ByVal dir As DirectionsEnum, Optional ByVal bBrackets As Boolean = True, Optional ByVal bRealRE As Boolean = False) As String
        Dim sRE As String = Adventure.sDirectionsRE(dir).ToLower
        If bRealRE Then sRE = sRE.Replace("/", "|")

        If bBrackets Then
            If bRealRE Then
                Return "(" & sRE & ")"
            Else
                Return "[" & sRE & "]"
            End If
        Else
            Return sRE
        End If
    End Function

    Public Function KeyExists(ByVal sKey As String) As Boolean
        With Adventure
            If .htblLocations.ContainsKey(sKey) Then Return True
            If .htblObjects.ContainsKey(sKey) Then Return True
            If .htblTasks.ContainsKey(sKey) Then Return True
            If .htblEvents.ContainsKey(sKey) Then Return True
            If .htblCharacters.ContainsKey(sKey) Then Return True
            If .htblGroups.ContainsKey(sKey) Then Return True
            If .htblVariables.ContainsKey(sKey) Then Return True
            If .htblALRs.ContainsKey(sKey) Then Return True
            If .htblHints.ContainsKey(sKey) Then Return True
            If .htblAllProperties.ContainsKey(sKey) Then Return True
        End With
        Return False
    End Function

    Public Function FindProperty(ByVal arlStates As StringArrayList) As String
        Dim bMatch As Boolean

        For Each prop As clsProperty In Adventure.htblAllProperties.Values
            bMatch = True
            If prop.arlStates.Count = arlStates.Count Then
                For Each sState As String In arlStates
                    If Not prop.arlStates.Contains(sState) Then
                        bMatch = False
                        Exit For
                    End If
                Next
                If bMatch Then
                    Return prop.Key
                End If
            End If
        Next

        Return Nothing
    End Function

    Public Function ToProper(ByVal sText As String, Optional ByVal bForceRestLower As Boolean = True) As String
        If bForceRestLower Then
            Return sLeft(sText, 1).ToUpper & sRight(sText, sText.Length - 1).ToLower
        Else
            Return sLeft(sText, 1).ToUpper & sRight(sText, sText.Length - 1)
        End If
    End Function

    Public Function FunctionNames() As StringArrayList
        Dim Names As New StringArrayList
        For Each sName As String In New String() {"AloneWithChar", "CharacterDescriptor", "CharacterName", "CharacterProper", "ConvCharacter", "DisplayCharacter", "DisplayLocation", "DisplayObject", "Held", "LCase", "ListCharactersOn", "ListCharactersIn", "ListCharactersOnAndIn", "ListHeld", "ListExits", "ListObjectsAtLocation", "ListWorn", "ListObjectsOn", "ListObjectsIn", "ListObjectsOnAndIn", "LocationName", "LocationOf", "NumberAsText", "ObjectName", "ObjectsIn", "ParentOf", "PCase", "Player", "PopUpChoice", "PopUpInput", "PrevListObjectsOn", "PrevParentOf", "ProperName", "PropertyValue", "Release", "Replace", "Sum", "TaskCompleted", "TheObject", "TheObjects", "Turns", "UCase", "Version", "Worn"}
            Names.Add(sName)
        Next
        If Adventure IsNot Nothing Then
            For Each UDF As clsUserFunction In Adventure.htblUDFs.Values
                Names.Add(UDF.Name)
            Next
        End If
        Return Names
    End Function

    Public Function ReferenceNames() As String()
        Return New String() {"%object%", "%objects%", "%object1%", "%object2%", "%object3%", "%object4%", "%object5%", "%direction%", "%direction1%", "%direction2%", "%direction3%", "%direction4%", "%direction5%", "%character%", "%character1%", "%character2%", "%character3%", "%character4%", "%character5%", "%text%", "%text1%", "%text2%", "%text3%", "%text4%", "%text5%", "%number%", "%number1%", "%number2%", "%number3%", "%number4%", "%number5%", "%location%", "%location1%", "%location2%", "%location3%", "%location4%", "%location5%", "%item%", "%item1%", "%item2%", "%item3%", "%item4%", "%item5%"}
    End Function

    Public Function InstrCount(ByVal sText As String, ByVal sSubText As String) As Integer
        Dim iOffset As Integer = 1
        Dim iCount As Integer = 0

        While iOffset < sText.Length
            If sInstr(iOffset, sText, sSubText) > 0 Then
                iCount += 1
                iOffset = sInstr(iOffset, sText, sSubText) + 1
            Else
                Return iCount
            End If
        End While

        Return iCount
    End Function

    Public Function EvaluateExpression(ByVal sExpression As String) As String
        If sExpression = "" Then Return ""

        Dim var As New clsVariable
        var.Type = clsVariable.VariableTypeEnum.Text
        var.SetToExpression(sExpression)
        Return var.StringValue
    End Function
    Public Function EvaluateExpression(ByVal sExpression As String, ByVal bInt As Boolean) As Integer

        Dim var As New clsVariable
        var.Type = clsVariable.VariableTypeEnum.Numeric
        var.SetToExpression(sExpression)
        Return var.IntValue

    End Function


    Public Function ReplaceExpressions(ByVal sText As String) As String
        Dim reExp As New System.Text.RegularExpressions.Regex("<#(?<expression>.*?)#>", System.Text.RegularExpressions.RegexOptions.Singleline)
        For Each m As System.Text.RegularExpressions.Match In reExp.Matches(sText)
            sText = sText.Replace(m.Value, EvaluateExpression(m.Groups("expression").Value))
        Next
        Return sText
    End Function


    Public Function ReplaceALRs(ByVal sText As String, Optional ByVal bAutoCapitalise As Boolean = True) As String
        Try
            If sText = "" Then Return ""

            sText = ReplaceFunctions(sText)
            sText = ReplaceExpressions(sText)

            Dim bChanged As Boolean = False

            ' Replace ALRs
            For Each alr As clsALR In Adventure.htblALRs.Values
                If sText.Contains(alr.OldText) Then
                    Dim sALR As String = alr.NewText.ToString ' Get it in a variable in case we have DisplayOnce
                    If sText = sALR Then Exit For
                    sText = sText.Replace(alr.OldText, ReplaceALRs(sALR, False))
                End If
            Next

            ' Auto-capitalise - needs to happen after ALR replacements, as some replacements may be both intra and start of sentences
            If bAutoCapitalise Then 'AndAlso bChanged Then
                Dim reCap As New System.Text.RegularExpressions.Regex("^(?<cap>[a-z])|\n(?<cap>[a-z])|[a-z][\.\!\?] ( )?(?<cap>[a-z])")
                While reCap.IsMatch(sText)
                    Dim match As System.Text.RegularExpressions.Match = reCap.Match(sText)
                    Dim sCap As String = match.Groups("cap").Value
                    Dim iIndex As Integer = match.Groups("cap").Index
                    sText = sText.Substring(0, iIndex) & sCap.ToUpper & sText.Substring(iIndex + sCap.Length)
                    bChanged = True
                End While
            End If

            ' Do a second round of ALR replacements if we auto-capped anything, as user may want to replace the auto-capped version
            If bChanged Then
                For Each alr As clsALR In Adventure.htblALRs.Values
                    If sText.Contains(alr.OldText) Then
                        Dim sALR As String = alr.NewText.ToString ' Get it in a variable in case we have DisplayOnce
                        If sText = sALR Then Exit For
                        sText = sText.Replace(alr.OldText, ReplaceALRs(sALR, False))
                    End If
                Next
            End If

            Return sText

        Catch ex As Exception
            ErrMsg("ReplaceALRs error", ex)
            Return sText
        End Try
    End Function

    Public Function pSpace(ByRef sText As String) As String
        If sText Is Nothing OrElse sText.Length = 0 Then
            sText = ""
            Return sText
        Else
            If sText.EndsWith(vbLf) Then Return sText
        End If

        sText &= "  "
        Return sText
    End Function


    Public Sub DisplayError(ByVal sText As String)
        ErrMsg(sText)
    End Sub

    ' A case ignoring search
    Public Function Contains(ByVal sTextToSearchIn As String, ByVal sTextToFind As String, Optional ByVal bExactWord As Boolean = False, Optional ByVal iStart As Integer = 0) As Boolean
        sTextToFind = sTextToFind.Replace(".", "\.")
        Dim sPattern As String = If(bExactWord, "\b" & sTextToFind & "\b", sTextToFind).ToString
        Dim regex As New System.Text.RegularExpressions.Regex(sPattern, System.Text.RegularExpressions.RegexOptions.IgnoreCase)
        Return regex.IsMatch(sTextToSearchIn.Substring(iStart))
    End Function
    Public Function ContainsWord(ByVal sTextToSearchIn As String, ByVal sTextToFind As String) As Boolean
        Return Contains(sTextToSearchIn, sTextToFind, True)
    End Function

    Public Function PreviousFunction(ByVal sFunction As String, ByVal sArgs As String) As String
        With UserSession
            Dim sNewFunction As String = sFunction.Replace("prev", "")
            Dim PreviousState As clsGameState = CType(.States.Peek, clsGameState) ' Note the previous state
            .States.RecordState() ' Save where we are now
            .States.RestoreState(PreviousState) ' Load up the previous state

            PreviousFunction = ReplaceFunctions("%" & sNewFunction & "[" & sArgs & "]%")

            .States.Pop() ' Get rid of the 'current' state and load it back as present
        End With
    End Function

    ' Chops the last character off a string 
    Public Function ChopLast(ByVal sText As String) As String
        If sText.Length > 0 Then
            Return sText.Substring(0, sText.Length - 1)
        Else
            Return ""
        End If
    End Function

    ' If this is in an expression, we need to replace anything with a quoted value
    Public Function ReplaceOO(ByVal sText As String, ByVal bExpression As Boolean) As String

        Dim reIgnore As New System.Text.RegularExpressions.Regex(".*?(?<embeddedexpression><#.*?#>).*?")
        If reIgnore.IsMatch(sText) Then
            Dim sMatch As String = reIgnore.Match(sText).Groups("embeddedexpression").Value
            Dim sGUID As String
            sGUID = ":" & System.Guid.NewGuid.ToString & ":"
            Dim sReturn As String = ReplaceOO(sText.Replace(sMatch, sGUID), bExpression)
            Return sReturn.Replace(sGUID, sMatch)
        Else
            ' Match anything unless it's between <# ... #> symbols
            Dim re As New System.Text.RegularExpressions.Regex("(?!<#.*?)(?<firstkey>%?[A-Za-z][\w\|_]*%?)(?<nextkey>\.%?[A-Za-z][\w\|_]*%?(\(.+?\))?)+(?!.*?#>)")
            Dim sPrevious As String = ""
            Dim iStartAt As Integer = 0

            While iStartAt < sText.Length AndAlso re.IsMatch(sText, iStartAt)  ' AndAlso sText <> sPrevious
                sPrevious = sText

                Dim sMatch As String = re.Match(sText, iStartAt).Value
                Dim bIntValue As Boolean = False
                Dim sReplace As String = ReplaceOOProperty(sMatch, bInt:=bIntValue)
                Dim iPrevStart As Integer = iStartAt
                iStartAt = re.Match(sText, iStartAt).Index + sMatch.Length

                If sReplace <> "#*!~#" Then
                    If sReplace.Contains(sMatch) Then sReplace = sReplace.Replace(sMatch, "*** RECURSIVE REPLACE ***")
                    If bExpression AndAlso Not bIntValue AndAlso Not Contains(sText, """" & sMatch & """", , iPrevStart + 1) Then sReplace = """" & sReplace & """"
                    sText = sText.Substring(0, iPrevStart) & Replace(sText, sMatch, sReplace, iPrevStart + 1, 1) 'sText = sText.Replace(sMatch, sReplace)
                    iStartAt += sReplace.Length - sMatch.Length
                End If

            End While

            Return ReplaceFunctions(sText, , False)
        End If
    End Function

    Private Function ReplaceOOProperty(ByVal sProperty As String, Optional ByVal ob As clsObject = Nothing, Optional ByVal ch As clsCharacter = Nothing, Optional ByVal loc As clsLocation = Nothing, Optional ByVal lst As List(Of clsItemWithProperties) = Nothing, Optional ByVal lstDirs As List(Of DirectionsEnum) = Nothing, Optional ByVal evt As clsEvent = Nothing, Optional sPropertyKey As String = Nothing, Optional ByRef bInt As Boolean = False) As String
        Dim sFunction As String = sProperty
        Dim sArgs As String = ""
        Dim sRemainder As String = ""

        Dim iIndexOfDot As Integer = sFunction.IndexOf(".")
        Dim iIndexOfOB As Integer = sFunction.IndexOf("(")

        If iIndexOfOB > -1 AndAlso sFunction.Contains(")") Then
            Dim iIndexOfCB As Integer = sFunction.IndexOf(")", iIndexOfOB)
            If iIndexOfDot > -1 Then
                If iIndexOfDot < iIndexOfOB Then
                    sRemainder = sFunction.Substring(iIndexOfDot + 1)
                    sFunction = sFunction.Substring(0, iIndexOfDot)
                Else
                    sArgs = sFunction.Substring(iIndexOfOB + 1, iIndexOfCB - iIndexOfOB - 1) '.ToLower
                    sRemainder = sFunction.Substring(iIndexOfCB + 2)
                    sFunction = sFunction.Substring(0, iIndexOfOB)
                End If
            Else
                sArgs = sFunction.Substring(iIndexOfOB + 1, sFunction.LastIndexOf(")") - iIndexOfOB - 1) '.ToLower
                sRemainder = sFunction.Substring(iIndexOfCB + 1)
                sFunction = sFunction.Substring(0, iIndexOfOB)
            End If
        ElseIf iIndexOfDot > 0 Then
            sRemainder = sFunction.Substring(iIndexOfDot + 1)
            sFunction = sFunction.Substring(0, iIndexOfDot)
        End If

        If lst IsNot Nothing OrElse (lstDirs IsNot Nothing AndAlso (sFunction = "List" OrElse sFunction = "Count" OrElse sFunction = "Name" OrElse sFunction = "")) Then
            Select Case sFunction
                Case ""
                    Dim sResult As String = ""
                    If lst IsNot Nothing Then
                        For i As Integer = 0 To lst.Count - 1
                            sResult &= lst(i).Key
                            If i < lst.Count - 1 Then sResult &= "|"
                        Next
                    Else
                        For i As Integer = 0 To lstDirs.Count - 1
                            sResult &= lstDirs(i).ToString
                            If i < lstDirs.Count - 1 Then sResult &= "|"
                        Next
                    End If

                    Return sResult

                Case "Count"
                    If lst IsNot Nothing Then
                        Return lst.Count.ToString
                    ElseIf lstDirs IsNot Nothing Then
                        Return lstDirs.Count.ToString
                    End If
                    bInt = True
                    Return "0"

                Case "Sum"
                    Dim iSum As Integer = 0
                    If lst IsNot Nothing AndAlso sPropertyKey IsNot Nothing Then
                        For Each oItem As clsItemWithProperties In lst
                            If oItem.HasProperty(sPropertyKey) Then
                                iSum += oItem.htblProperties(sPropertyKey).IntData
                            End If
                        Next
                    End If
                    bInt = True
                    Return iSum.ToString

                Case "Description"
                    Dim sResult As String = ""
                    For Each oItem As clsItem In lst
                        pSpace(sResult)
                        Select Case True
                            Case TypeOf oItem Is clsObject
                                sResult &= CType(oItem, clsObject).Description.ToString
                            Case TypeOf oItem Is clsCharacter
                                sResult &= CType(oItem, clsCharacter).Description.ToString
                            Case TypeOf oItem Is clsLocation
                                If CType(oItem, clsLocation).ViewLocation = "" Then
                                    sResult &= "There is nothing of interest here."
                                Else
                                    sResult &= CType(oItem, clsLocation).ViewLocation
                                End If
                        End Select
                    Next
                    Return sResult

                Case "List", "Name"
                    ' List(and) - And separated list - Default
                    ' List(or) - Or separated list
                    Dim sSeparator As String = " and "
                    Dim sArgsTest As String = sArgs.ToLower

                    If sArgsTest.Contains("or") Then sSeparator = " or "
                    If sArgsTest.Contains("rows") Then sSeparator = vbCrLf

                    ' List(definite/the) - List the objects names - Default
                    ' List(indefinite) - List a/an object
                    Dim Article As ArticleTypeEnum = ArticleTypeEnum.Definite
                    If sArgsTest.Contains("indefinite") Then Article = ArticleTypeEnum.Indefinite
                    If sArgsTest.Contains("none") Then Article = ArticleTypeEnum.None

                    ' List(true) - List anything in/on everything in the list (single level) - Default
                    ' List(false) - Do not list anything in/on
                    Dim bListInOn As Boolean = True ' List any objects in or on anything in this list
                    If sFunction = "Name" OrElse sArgsTest.Contains("false") OrElse sArgsTest.Contains("0") Then
                        bListInOn = False
                    End If

                    Dim bForcePronoun As Boolean = False
                    Dim ePronoun As PronounEnum = PronounEnum.Subjective
                    If sArgsTest.Contains("none") Then ePronoun = PronounEnum.None
                    If sArgsTest.Contains("force") Then bForcePronoun = True
                    If sArgsTest.Contains("objective") OrElse sArgsTest.Contains("object") OrElse sArgsTest.Contains("target") Then ePronoun = PronounEnum.Objective
                    If sArgsTest.Contains("possessive") OrElse sArgsTest.Contains("possess") Then ePronoun = PronounEnum.Possessive
                    If sArgsTest.Contains("reflective") OrElse sArgsTest.Contains("reflect") Then ePronoun = PronounEnum.Reflective

                    Dim sList As String = ""
                    Dim i As Integer = 0
                    If lst IsNot Nothing Then
                        For Each oItem As clsItem In lst
                            i += 1
                            If sSeparator = vbCrLf Then
                                If i > 1 Then sList &= sSeparator
                            Else
                                If i > 1 AndAlso i < lst.Count Then sList &= ", "
                                If lst.Count > 1 AndAlso i = lst.Count Then sList &= sSeparator
                            End If

                            If TypeOf oItem Is clsObject Then
                                sList &= CType(oItem, clsObject).FullName(Article)
                                If bListInOn AndAlso CType(oItem, clsObject).Children(clsObject.WhereChildrenEnum.InsideOrOnObject).Count > 0 Then sList &= ".  " & ChopLast(CType(oItem, clsObject).DisplayObjectChildren)
                            ElseIf TypeOf oItem Is clsCharacter Then
                                Article = ArticleTypeEnum.Indefinite ' opposite default from objects
                                If sArgsTest.Contains("definite") Then Article = ArticleTypeEnum.Definite
                                sList &= CType(oItem, clsCharacter).Name(ePronoun, , , Article, bForcePronoun)
                            ElseIf TypeOf oItem Is clsLocation Then
                                sList &= CType(oItem, clsLocation).ShortDescription.ToString
                            End If
                        Next
                    ElseIf lstDirs IsNot Nothing Then
                        For Each oDir As DirectionsEnum In lstDirs
                            i += 1
                            If i > 1 AndAlso i < lstDirs.Count Then sList &= ", "
                            If lstDirs.Count > 1 AndAlso i = lstDirs.Count Then sList &= sSeparator
                            sList &= LCase(DirectionName(oDir))
                        Next
                    End If
                    If sList = "" Then sList = "nothing"
                    Return sList

                Case "Parent"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    Dim lstKeys As New List(Of String)

                    For Each oItem As clsItemWithProperties In lst
                        Dim sParent As String = oItem.Parent
                        If sParent <> "" Then
                            If Not lstKeys.Contains(sParent) Then
                                Dim oItemNew As clsItemWithProperties = CType(Adventure.dictAllItems(sParent), clsItemWithProperties)
                                lstKeys.Add(sParent)
                                lstNew.Add(oItemNew)
                            End If
                        End If
                    Next

                    If sRemainder <> "" Then
                        Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)
                    End If

                Case "Children"
                    Dim lstNew As New List(Of clsItemWithProperties)

                    For Each oItem As clsItemWithProperties In lst
                        Select Case True
                            Case TypeOf oItem Is clsObject
                                ob = CType(oItem, clsObject)
                                Select Case sArgs.ToLower.Replace(" ", "")
                                    Case "", "all", "onandin", "all,onandin"
                                        For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideOrOnObject, True).Values
                                            lstNew.Add(oOb)
                                        Next
                                        For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideOrOnObject).Values
                                            lstNew.Add(oCh)
                                        Next
                                    Case "characters,in"
                                        For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).Values
                                            lstNew.Add(oCh)
                                        Next
                                    Case "characters,on"
                                        For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.OnObject).Values
                                            lstNew.Add(oCh)
                                        Next
                                    Case "characters,onandin", "characters"
                                        For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideOrOnObject).Values
                                            lstNew.Add(oCh)
                                        Next
                                    Case "in"
                                        For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideObject, True).Values
                                            lstNew.Add(oOb)
                                        Next
                                        For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).Values
                                            lstNew.Add(oCh)
                                        Next
                                    Case "objects,in"
                                        For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideObject, True).Values
                                            lstNew.Add(oOb)
                                        Next
                                    Case "objects,on"
                                        For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.OnObject, True).Values
                                            lstNew.Add(oOb)
                                        Next
                                    Case "objects,onandin", "objects"
                                        For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideOrOnObject, True).Values
                                            lstNew.Add(oOb)
                                        Next
                                End Select

                            Case TypeOf oItem Is clsCharacter
                                ' No real reason we couldn't give Children from a Character

                        End Select
                    Next

                    If sRemainder <> "" OrElse lstNew.Count > 0 Then
                        Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)
                    End If

                Case "Contents"
                    Dim lstNew As New List(Of clsItemWithProperties)

                    For Each oItem As clsItemWithProperties In lst
                        Select Case True
                            Case TypeOf oItem Is clsObject
                                ob = CType(oItem, clsObject)
                                Select Case sArgs.ToLower
                                    Case "", "all"
                                        For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideObject, True).Values
                                            lstNew.Add(oOb)
                                        Next
                                        For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).Values
                                            lstNew.Add(oCh)
                                        Next
                                    Case "characters"
                                        For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).Values
                                            lstNew.Add(oCh)
                                        Next
                                    Case "objects"
                                        For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideObject, True).Values
                                            lstNew.Add(oOb)
                                        Next
                                End Select
                        End Select
                    Next

                    Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)

                Case "Objects"
                    Dim lstNew As New List(Of clsItemWithProperties)

                    For Each oItem As clsItemWithProperties In lst
                        Select Case True
                            Case TypeOf oItem Is clsLocation
                                loc = CType(oItem, clsLocation)
                                For Each obLoc As clsObject In loc.ObjectsInLocation.Values
                                    lstNew.Add(obLoc)
                                Next
                            Case Else
                                TODO()
                        End Select
                    Next

                    Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)

                Case Else
                    If Adventure.htblAllProperties.ContainsKey(sFunction) Then
                        Dim lstNew As New List(Of clsItemWithProperties)
                        Dim lstKeys As New List(Of String)
                        Dim iTotal As Integer = 0
                        Dim sResult As String = ""
                        Dim bIntResult As Boolean = False
                        Dim bPropertyFound As Boolean = False
                        Dim sNewPropertyKey As String = Nothing

                        For Each oItem As clsItemWithProperties In lst
                            If oItem.HasProperty(sFunction) Then
                                bPropertyFound = True
                                Dim p As clsProperty = oItem.GetProperty(sFunction)

                                Dim bValueOK As Boolean = True
                                If sArgs <> "" Then
                                    bValueOK = False
                                    If sArgs.Contains(".") Then sArgs = ReplaceFunctions(sArgs)
                                    If sArgs.Contains("+") OrElse sArgs.Contains("-") Then
                                        Dim sArgsNew As String = EvaluateExpression(sArgs)
                                        If sArgsNew IsNot Nothing Then sArgs = sArgsNew
                                    End If
                                    Select Case p.Type
                                        Case clsProperty.PropertyTypeEnum.ValueList
                                            Dim iCheckValue As Integer = 0
                                            If IsNumeric(sArgs) Then
                                                iCheckValue = SafeInt(sArgs)
                                            Else
                                                If p.ValueList.ContainsKey(sArgs) Then iCheckValue = p.ValueList(sArgs)
                                            End If
                                            bValueOK = p.IntData = iCheckValue
                                            bInt = True
                                        Case clsProperty.PropertyTypeEnum.Integer
                                            Dim iCheckValue As Integer = 0
                                            If IsNumeric(sArgs) Then iCheckValue = SafeInt(sArgs)
                                            bValueOK = p.IntData = iCheckValue
                                            bInt = True
                                        Case clsProperty.PropertyTypeEnum.StateList
                                            bValueOK = p.Value.ToLower = sArgs.ToLower
                                        Case clsProperty.PropertyTypeEnum.SelectionOnly
                                            Select Case sArgs.ToLower
                                                Case "false", "0"
                                                    bValueOK = False
                                                Case Else
                                                    bValueOK = True
                                            End Select
                                            bInt = True
                                        Case Else
                                            TODO("Property filter check not yet implemented for " & p.Type.ToString)
                                    End Select
                                End If

                                If bValueOK Then
                                    Select Case p.Type
                                        Case clsProperty.PropertyTypeEnum.CharacterKey
                                            Dim chP As clsCharacter = Adventure.htblCharacters(p.Value)
                                            If Not lstKeys.Contains(chP.Key) Then
                                                lstKeys.Add(chP.Key)
                                                lstNew.Add(chP)
                                            End If
                                        Case clsProperty.PropertyTypeEnum.LocationGroupKey
                                            For Each sItem As String In Adventure.htblGroups(p.Value).arlMembers
                                                If Not lstKeys.Contains(sItem) Then
                                                    Dim oItemNew As clsItemWithProperties = CType(Adventure.dictAllItems(sItem), clsItemWithProperties)
                                                    lstKeys.Add(oItemNew.Key)
                                                    lstNew.Add(oItemNew)
                                                End If
                                            Next
                                        Case clsProperty.PropertyTypeEnum.LocationKey
                                            Dim oItemNew As clsItemWithProperties = Adventure.htblLocations(p.Value)
                                            If Not lstKeys.Contains(oItemNew.Key) Then
                                                lstKeys.Add(oItemNew.Key)
                                                lstNew.Add(oItemNew)
                                            End If
                                        Case clsProperty.PropertyTypeEnum.ObjectKey
                                            Dim oItemNew As clsItemWithProperties = Adventure.htblObjects(p.Value)
                                            If Not lstKeys.Contains(oItemNew.Key) Then
                                                lstKeys.Add(oItemNew.Key)
                                                lstNew.Add(oItemNew)
                                            End If
                                        Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.ValueList, clsProperty.PropertyTypeEnum.StateList
                                            lstNew.Add(oItem)
                                            sNewPropertyKey = sFunction
                                            bInt = p.Type = clsProperty.PropertyTypeEnum.Integer OrElse p.Type = clsProperty.PropertyTypeEnum.ValueList
                                        Case clsProperty.PropertyTypeEnum.SelectionOnly
                                            ' Selection Only property to further reduce list
                                            lstNew.Add(oItem)
                                            bInt = True
                                        Case clsProperty.PropertyTypeEnum.Text
                                            If sResult <> "" Then
                                                If oItem Is lst(lst.Count - 1) Then sResult &= " and " Else sResult &= ", "
                                            End If
                                            sResult &= p.Value
                                    End Select
                                End If
                            Else
                                If Adventure.htblAllProperties.ContainsKey(sFunction) Then
                                    Dim p As clsProperty = Adventure.htblAllProperties(sFunction)
                                    Dim bValueOK As Boolean = False ' Because this is equiv of arg = (True)
                                    If sArgs <> "" Then
                                        bValueOK = False
                                        If sArgs.Contains(".") Then sArgs = ReplaceFunctions(sArgs)
                                        If sArgs.Contains("+") OrElse sArgs.Contains("-") Then
                                            Dim sArgsNew As String = EvaluateExpression(sArgs)
                                            If sArgsNew IsNot Nothing Then sArgs = sArgsNew
                                        End If
                                        Select Case p.Type
                                            ' Opposite of above, since this item _doesn't_ contain this property
                                            Case clsProperty.PropertyTypeEnum.SelectionOnly
                                                Select Case sArgs.ToLower
                                                    Case "false", "0"
                                                        bValueOK = True
                                                    Case Else
                                                        bValueOK = False
                                                End Select
                                                bInt = True
                                            Case clsProperty.PropertyTypeEnum.StateList
                                                bValueOK = False ' Since we don't have the property
                                            Case Else
                                                TODO("Property filter check not yet implemented for " & p.Type.ToString)
                                        End Select
                                    End If
                                    If bValueOK Then
                                        Select Case p.Type
                                            Case clsProperty.PropertyTypeEnum.SelectionOnly
                                                ' Selection Only property to further reduce list
                                                lstNew.Add(oItem)
                                        End Select
                                    End If
                                End If
                            End If
                        Next

                        If Not bPropertyFound Then
                            Select Case Adventure.htblAllProperties(sFunction).Type
                                Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.ValueList
                                    bIntResult = True
                                    bInt = True
                            End Select
                        End If

                        If sRemainder <> "" OrElse (lstNew.Count > 0 AndAlso Not bIntResult) Then
                            Return ReplaceOOProperty(sRemainder, lst:=lstNew, sPropertyKey:=sNewPropertyKey, bInt:=bInt)
                        ElseIf bIntResult Then
                            Return iTotal.ToString
                        Else
                            Return sResult
                        End If
                    End If
            End Select

        ElseIf ob IsNot Nothing Then
            Select Case sFunction
                Case "Adjective", "Prefix"
                    Return ob.Prefix

                Case "Article"
                    Return ob.Article

                Case "Children"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    Select Case sArgs.ToLower.Replace(" ", "")
                        Case "", "all", "onandin", "all,onandin"
                            For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideOrOnObject, True).Values
                                lstNew.Add(oOb)
                            Next
                            For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideOrOnObject).Values
                                lstNew.Add(oCh)
                            Next
                        Case "characters,in"
                            For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).Values
                                lstNew.Add(oCh)
                            Next
                        Case "characters,on"
                            For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.OnObject).Values
                                lstNew.Add(oCh)
                            Next
                        Case "characters,onandin", "characters"
                            For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideOrOnObject).Values
                                lstNew.Add(oCh)
                            Next
                        Case "in"
                            For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideObject, True).Values
                                lstNew.Add(oOb)
                            Next
                            For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).Values
                                lstNew.Add(oCh)
                            Next
                        Case "objects,in"
                            For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideObject, True).Values
                                lstNew.Add(oOb)
                            Next
                        Case "objects,on"
                            For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.OnObject, True).Values
                                lstNew.Add(oOb)
                            Next
                        Case "objects,onandin", "objects"
                            For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideOrOnObject, True).Values
                                lstNew.Add(oOb)
                            Next
                    End Select
                    Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)

                Case "Contents"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    Select Case sArgs.ToLower
                        Case "", "all"
                            For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideObject, True).Values
                                lstNew.Add(oOb)
                            Next
                            For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).Values
                                lstNew.Add(oCh)
                            Next
                        Case "characters"
                            For Each oCh As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).Values
                                lstNew.Add(oCh)
                            Next
                        Case "objects"
                            For Each oOb As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideObject, True).Values
                                lstNew.Add(oOb)
                            Next
                    End Select
                    Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)

                Case "Count"
                    bInt = True
                    Return "1"

                Case "Description"
                    Return ob.Description.ToString

                Case "Location"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    For Each sLocKey As String In ob.LocationRoots.Keys
                        Dim oLoc As clsLocation = Adventure.htblLocations(sLocKey)
                        lstNew.Add(oLoc)
                    Next
                    Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)

                Case "Name", "List"
                    Dim Article As ArticleTypeEnum = ArticleTypeEnum.Definite
                    If sArgs.ToLower.Contains("indefinite") Then Article = ArticleTypeEnum.Indefinite
                    If sArgs.ToLower.Contains("none") Then Article = ArticleTypeEnum.None

                    Return ob.FullName(Article)

                Case "Noun"
                    Return ob.arlNames(0)

                Case "Parent"
                    Dim sParent As String = ob.Parent
                    Dim oParentOb As clsObject = Nothing
                    Dim oParentCh As clsCharacter = Nothing
                    Dim oParentLoc As clsLocation = Nothing
                    Adventure.htblObjects.TryGetValue(sParent, oParentOb)
                    Adventure.htblCharacters.TryGetValue(sParent, oParentCh)
                    Adventure.htblLocations.TryGetValue(sParent, oParentLoc)
                    Return ReplaceOOProperty(sRemainder, oParentOb, oParentCh, oParentLoc, bInt:=bInt)

                Case ""
                    Return ob.Key

                Case Else
                    Dim p As clsProperty = Nothing
                    Dim oOb As clsObject = Nothing
                    Dim oCh As clsCharacter = Nothing
                    Dim oLoc As clsLocation = Nothing
                    Dim lstNew As List(Of clsItemWithProperties) = Nothing

                    If ob.htblProperties.TryGetValue(sFunction, p) Then
                        Select Case p.Type
                            Case clsProperty.PropertyTypeEnum.CharacterKey
                                oCh = Adventure.htblCharacters(p.Value)
                            Case clsProperty.PropertyTypeEnum.LocationGroupKey
                                If lstNew Is Nothing Then lstNew = New List(Of clsItemWithProperties)
                                For Each sItem As String In Adventure.htblGroups(p.Value).arlMembers
                                    Dim oItem As clsItemWithProperties = CType(Adventure.dictAllItems(sItem), clsItemWithProperties)
                                    lstNew.Add(oItem)
                                Next
                            Case clsProperty.PropertyTypeEnum.LocationKey
                                oLoc = Adventure.htblLocations(p.Value)
                            Case clsProperty.PropertyTypeEnum.ObjectKey
                                oOb = Adventure.htblObjects(p.Value)
                            Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.ValueList
                                Return p.IntData.ToString
                            Case clsProperty.PropertyTypeEnum.SelectionOnly
                                Return "1"
                            Case clsProperty.PropertyTypeEnum.Text, clsProperty.PropertyTypeEnum.StateList
                                Return p.Value
                        End Select
                    Else
                        If Adventure.htblObjectProperties.ContainsKey(sFunction) Then
                            ' Ok, item doesn't have property.  Give it a default
                            Select Case Adventure.htblObjectProperties(sFunction).Type
                                Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.ValueList, clsProperty.PropertyTypeEnum.SelectionOnly
                                    bInt = True
                                    Return "0"
                            End Select
                        End If
                    End If
                    If sRemainder <> "" Then
                        Return ReplaceOOProperty(sRemainder, oOb, oCh, oLoc, lstNew, bInt:=bInt)
                    ElseIf oLoc IsNot Nothing Then
                        Return oLoc.Key
                    ElseIf oOb IsNot Nothing Then
                        Return oOb.Key
                    ElseIf oCh IsNot Nothing Then
                        Return oCh.Key
                    ElseIf lstNew IsNot Nothing AndAlso lstNew.Count > 0 Then
                        Return ReplaceOOProperty(sRemainder, oOb, oCh, oLoc, lstNew, bInt:=bInt)
                    End If

            End Select

        ElseIf ch IsNot Nothing Then
            Select Case sFunction
                Case "Count"
                    bInt = True
                    Return "1"

                Case "Descriptor"
                    Return ch.Descriptor.ToString

                Case "Description"
                    Return ch.Description.ToString

                Case "Exits"
                    Dim lstNew As New List(Of DirectionsEnum)

                    For d As DirectionsEnum = DirectionsEnum.North To DirectionsEnum.NorthWest
                        If Adventure.Player.HasRouteInDirection(d, False, Adventure.Player.Location.LocationKey) Then
                            lstNew.Add(d)
                        End If
                    Next
                    Return ReplaceOOProperty(sRemainder, , ch, , , lstNew, bInt:=bInt)

                Case "Held"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    Select Case sArgs.ToLower
                        Case "", "true", "1", "-1"
                            For Each obHeld As clsObject In ch.HeldObjects(True).Values
                                lstNew.Add(obHeld)
                            Next
                        Case "false", "0"
                            For Each obHeld As clsObject In ch.HeldObjects(False).Values
                                lstNew.Add(obHeld)
                            Next
                    End Select
                    Return ReplaceOOProperty(sRemainder, , ch, , lstNew, bInt:=bInt)

                Case "Location"
                    Dim sLoc As String = ch.Location.LocationKey
                    Dim oLoc As clsLocation = Nothing
                    Adventure.htblLocations.TryGetValue(sLoc, oLoc)
                    Return ReplaceOOProperty(sRemainder, , , oLoc, bInt:=bInt)

                Case "Name"
                    Dim bForcePronoun As Boolean = False

                    Dim bTheNames As Boolean = False ' opposite default from objects
                    Dim Article As ArticleTypeEnum = ArticleTypeEnum.Definite
                    Dim bExplicitArticle As Boolean = False
                    Dim ePronoun As PronounEnum = PronounEnum.Subjective
                    Dim sArgsTest As String = sArgs.ToLower

                    If sArgsTest.Contains("none") Then
                        ' Could mean either article or pronoun
                        If sArgsTest.Contains("definite") OrElse sArgsTest.Contains("indefinite") Then
                            ePronoun = PronounEnum.None
                        ElseIf sArgsTest.Contains("force") OrElse sArgsTest.Contains("objective") OrElse sArgsTest.Contains("possessive") OrElse sArgsTest.Contains("reflective") Then
                            Article = ArticleTypeEnum.None
                        Else
                            ' If only None is specified, assume they mean pronouns, as less likely they'll disable articles on character names
                            ePronoun = PronounEnum.None
                        End If
                    End If
                    If sArgsTest.Contains("force") Then bForcePronoun = True
                    If sArgsTest.Contains("objective") OrElse sArgsTest.Contains("object") OrElse sArgsTest.Contains("target") Then ePronoun = PronounEnum.Objective
                    If sArgsTest.Contains("possessive") OrElse sArgsTest.Contains("possess") Then ePronoun = PronounEnum.Possessive
                    If sArgsTest.Contains("reflective") OrElse sArgsTest.Contains("reflect") Then ePronoun = PronounEnum.Reflective
                    'If sArgsTest.Contains("definite") Then Article = ArticleTypeEnum.Definite
                    If ContainsWord(sArgsTest, "definite") Then
                        Article = ArticleTypeEnum.Definite
                        bExplicitArticle = True
                    End If
                    If ContainsWord(sArgsTest, "indefinite") Then
                        Article = ArticleTypeEnum.Indefinite
                        bExplicitArticle = True
                    End If

                    Return ch.Name(ePronoun, , , Article, bForcePronoun, bExplicitArticle)

                Case "Parent"
                    Dim sParent As String = ch.Parent
                    Dim oParentOb As clsObject = Nothing
                    Dim oParentCh As clsCharacter = Nothing
                    Dim oParentLoc As clsLocation = Nothing
                    Adventure.htblObjects.TryGetValue(sParent, oParentOb)
                    Adventure.htblCharacters.TryGetValue(sParent, oParentCh)
                    Adventure.htblLocations.TryGetValue(sParent, oParentLoc)
                    Return ReplaceOOProperty(sRemainder, oParentOb, oParentCh, oParentLoc, bInt:=bInt)

                Case "ProperName"
                    Return ch.ProperName

                Case "Worn"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    Select Case sArgs.ToLower
                        Case "", "true", "1", "-1"
                            For Each obWorn As clsObject In ch.WornObjects(True).Values
                                lstNew.Add(obWorn)
                            Next
                        Case "false", "0"
                            For Each obWorn As clsObject In ch.WornObjects(False).Values
                                lstNew.Add(obWorn)
                            Next
                    End Select
                    Return ReplaceOOProperty(sRemainder, , ch, , lstNew, bInt:=bInt)

                Case "WornAndHeld"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    Select Case sArgs.ToLower
                        Case "", "true", "1", "-1"
                            For Each obWorn As clsObject In ch.WornObjects(True).Values
                                lstNew.Add(obWorn)
                            Next
                            For Each obHeld As clsObject In ch.HeldObjects(True).Values
                                lstNew.Add(obHeld)
                            Next
                        Case "false", "0"
                            For Each obWorn As clsObject In ch.WornObjects(False).Values
                                lstNew.Add(obWorn)
                            Next
                            For Each obHeld As clsObject In ch.HeldObjects(False).Values
                                lstNew.Add(obHeld)
                            Next
                    End Select
                    Return ReplaceOOProperty(sRemainder, , ch, , lstNew, bInt:=bInt)

                Case ""
                    Return ch.Key

                Case Else
                    Dim p As clsProperty = Nothing
                    Dim oOb As clsObject = Nothing
                    Dim oCh As clsCharacter = Nothing
                    Dim oLoc As clsLocation = Nothing
                    Dim lstNew As List(Of clsItemWithProperties) = Nothing

                    If ch.htblProperties.TryGetValue(sFunction, p) Then
                        Select Case p.Type
                            Case clsProperty.PropertyTypeEnum.CharacterKey
                                oCh = Adventure.htblCharacters(p.Value)
                            Case clsProperty.PropertyTypeEnum.LocationGroupKey
                                lstNew = New List(Of clsItemWithProperties)
                                For Each sItem As String In Adventure.htblGroups(p.Value).arlMembers
                                    Dim oItem As clsItemWithProperties = CType(Adventure.dictAllItems(sItem), clsItemWithProperties)
                                    lstNew.Add(oItem)
                                Next
                            Case clsProperty.PropertyTypeEnum.LocationKey
                                oLoc = Adventure.htblLocations(p.Value)
                            Case clsProperty.PropertyTypeEnum.ObjectKey
                                oOb = Adventure.htblObjects(p.Value)
                            Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.ValueList
                                bInt = True
                                Return p.IntData.ToString
                            Case clsProperty.PropertyTypeEnum.SelectionOnly
                                bInt = True
                                Return "1"
                            Case clsProperty.PropertyTypeEnum.Text, clsProperty.PropertyTypeEnum.StateList
                                Return p.Value
                        End Select
                    Else
                        If Adventure.htblCharacterProperties.ContainsKey(sFunction) Then
                            ' Ok, item doesn't have property.  Give it a default
                            Select Case Adventure.htblCharacterProperties(sFunction).Type
                                Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.ValueList, clsProperty.PropertyTypeEnum.SelectionOnly
                                    Return "0"
                            End Select
                        End If
                    End If
                    If sRemainder <> "" Then
                        Return ReplaceOOProperty(sRemainder, oOb, oCh, oLoc, lstNew, bInt:=bInt)
                    ElseIf oLoc IsNot Nothing Then
                        Return oLoc.Key
                    ElseIf oOb IsNot Nothing Then
                        Return oOb.Key
                    ElseIf oCh IsNot Nothing Then
                        Return oCh.Key
                    ElseIf lstNew IsNot Nothing AndAlso lstNew.Count > 0 Then
                        Return ReplaceOOProperty(sRemainder, oOb, oCh, oLoc, lstNew, bInt:=bInt)
                    End If

            End Select

        ElseIf loc IsNot Nothing Then
            Select Case sFunction
                Case "Characters"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    For Each chLoc As clsCharacter In loc.CharactersVisibleAtLocation.Values
                        lstNew.Add(chLoc)
                    Next
                    Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)

                Case "Contents"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    Select Case sArgs.ToLower
                        Case "", "all"
                            For Each oOb As clsObject In loc.ObjectsInLocation.Values
                                lstNew.Add(oOb)
                            Next
                            For Each oCh As clsCharacter In loc.CharactersDirectlyInLocation.Values
                                lstNew.Add(oCh)
                            Next
                        Case "characters"
                            For Each oCh As clsCharacter In loc.CharactersDirectlyInLocation.Values
                                lstNew.Add(oCh)
                            Next
                        Case "objects"
                            For Each oOb As clsObject In loc.ObjectsInLocation.Values
                                lstNew.Add(oOb)
                            Next
                    End Select
                    Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)

                Case "Count"
                    bInt = True
                    Return "1"

                Case "Description", LONGLOCATIONDESCRIPTION
                    Dim sResult As String = loc.ViewLocation
                    If sResult = "" Then sResult = "There is nothing of interest here."
                    Return sResult

                Case "Exits"
                    Dim lstNew As New List(Of DirectionsEnum)

                    For d As DirectionsEnum = DirectionsEnum.North To DirectionsEnum.NorthWest ' Adventure.iCompassPoints
                        If loc.arlDirections(d).LocationKey <> "" Then
                            lstNew.Add(d)
                        End If
                    Next
                    Return ReplaceOOProperty(sRemainder, , ch, , , lstNew, bInt:=bInt)

                Case "LocationTo"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    Dim locTo As clsLocation = Nothing

                    For Each sDir As String In sArgs.ToLower.Split("|"c)
                        For Each d As DirectionsEnum In [Enum].GetValues(GetType(DirectionsEnum))
                            If sDir = d.ToString.ToLower Then
                                Dim sLocTo As String = loc.arlDirections(d).LocationKey
                                If sLocTo IsNot Nothing Then
                                    locTo = Nothing
                                    Adventure.htblLocations.TryGetValue(sLocTo, locTo)
                                    If locTo IsNot Nothing Then
                                        lstNew.Add(locTo)
                                    End If
                                End If
                                Exit For
                            End If
                        Next
                    Next

                    If lstNew.Count = 1 Then
                        locTo = CType(lstNew(0), clsLocation)
                        lstNew = Nothing
                    End If
                    Return ReplaceOOProperty(sRemainder, , , locTo, lstNew, bInt:=bInt)

                Case "Name", SHORTLOCATIONDESCRIPTION
                    Return loc.ShortDescription.ToString

                Case "Objects"
                    Dim lstNew As New List(Of clsItemWithProperties)
                    For Each obLoc As clsObject In loc.ObjectsInLocation.Values
                        lstNew.Add(obLoc)
                    Next
                    Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)

                Case ""
                    Return loc.Key

                Case Else
                    Dim p As clsProperty = Nothing
                    Dim oOb As clsObject = Nothing
                    Dim oCh As clsCharacter = Nothing
                    Dim oLoc As clsLocation = Nothing
                    Dim lstNew As List(Of clsItemWithProperties)

                    If loc.htblProperties.TryGetValue(sFunction, p) Then
                        Select Case p.Type
                            Case clsProperty.PropertyTypeEnum.CharacterKey
                                oCh = Adventure.htblCharacters(p.Value)
                            Case clsProperty.PropertyTypeEnum.LocationGroupKey
                                lstNew = New List(Of clsItemWithProperties)
                                For Each sItem As String In Adventure.htblGroups(p.Value).arlMembers
                                    Dim oItem As clsItemWithProperties = CType(Adventure.dictAllItems(sItem), clsItemWithProperties)
                                    lstNew.Add(oItem)
                                Next
                            Case clsProperty.PropertyTypeEnum.LocationKey
                                oLoc = Adventure.htblLocations(p.Value)
                            Case clsProperty.PropertyTypeEnum.ObjectKey
                                oOb = Adventure.htblObjects(p.Value)
                            Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.ValueList
                                bInt = True
                                Return p.IntData.ToString
                            Case clsProperty.PropertyTypeEnum.SelectionOnly
                                bInt = True
                                Return "1"
                            Case clsProperty.PropertyTypeEnum.Text, clsProperty.PropertyTypeEnum.StateList
                                Return p.Value
                        End Select
                    Else
                        If Adventure.htblLocationProperties.ContainsKey(sFunction) Then
                            ' Ok, item doesn't have property.  Give it a default
                            Select Case Adventure.htblLocationProperties(sFunction).Type
                                Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.ValueList, clsProperty.PropertyTypeEnum.SelectionOnly
                                    Return "0"
                            End Select
                        End If
                    End If
                    If sRemainder <> "" Then
                        Return ReplaceOOProperty(sRemainder, oOb, oCh, oLoc, lstNew, bInt:=bInt)
                    ElseIf oLoc IsNot Nothing Then
                        Return oLoc.Key
                    ElseIf oOb IsNot Nothing Then
                        Return oOb.Key
                    ElseIf oCh IsNot Nothing Then
                        Return oCh.Key
                    ElseIf lstNew.Count > 0 Then
                        Return ReplaceOOProperty(sRemainder, oOb, oCh, oLoc, lstNew, bInt:=bInt)
                    End If

            End Select

        ElseIf evt IsNot Nothing Then
            Select Case sFunction
                Case "Length"
                    Return evt.Length.Value.ToString
                Case "Position"
                    Return evt.TimerFromStartOfEvent.ToString
                Case ""
                    Return evt.Key

            End Select

        Else
            If sFunction.Contains("|") Then
                Dim lstNew As New List(Of clsItemWithProperties)
                For Each sItem As String In sFunction.Split("|"c)
                    Dim oItem As clsItemWithProperties = CType(Adventure.dictAllItems(sItem), clsItemWithProperties)
                    lstNew.Add(oItem)
                Next
                Return ReplaceOOProperty(sRemainder, , , , lstNew, bInt:=bInt)
            Else
                If Adventure.AllKeys.Contains(sFunction) Then
                    Dim lstNew As List(Of clsItemWithProperties) = Nothing
                    If Adventure.htblObjects.ContainsKey(sFunction) Then
                        ob = Adventure.htblObjects(sFunction)
                    ElseIf Adventure.htblCharacters.ContainsKey(sFunction) Then
                        ch = Adventure.htblCharacters(sFunction)
                    ElseIf Adventure.htblLocations.ContainsKey(sFunction) Then
                        loc = Adventure.htblLocations(sFunction)
                    ElseIf Adventure.htblEvents.ContainsKey(sFunction) Then
                        evt = Adventure.htblEvents(sFunction)
                    ElseIf Adventure.htblGroups.ContainsKey(sFunction) Then
                        Dim grp As clsGroup = Adventure.htblGroups(sFunction)
                        lstNew = New List(Of clsItemWithProperties)
                        For Each sMember As String In grp.arlMembers
                            Dim itm As clsItemWithProperties = CType(Adventure.dictAllItems(sMember), clsItemWithProperties)
                            lstNew.Add(itm)
                        Next
                    End If

                    Return ReplaceOOProperty(sRemainder, ob, ch, loc, lstNew, , evt, bInt:=bInt)
                Else
                    For Each d As DirectionsEnum In [Enum].GetValues(GetType(DirectionsEnum))
                        If d.ToString = sFunction Then
                            Dim NewDir As New List(Of DirectionsEnum)
                            NewDir.Add(d)
                            Return ReplaceOOProperty(sRemainder, lstDirs:=NewDir, bInt:=bInt)
                        End If
                    Next
                End If
            End If
        End If

        Return "#*!~#"

    End Function

    Private Function SplitArgs(ByVal sArgs As String) As List(Of String)
        Dim lArgs As New List(Of String)
        Dim iLevel As Integer = 0
        Dim sArg As String = ""

        For i As Integer = 0 To sArgs.Length - 1
            Select Case sArgs(i)
                Case ","c
                    If iLevel = 0 Then
                        If sArg <> "" Then lArgs.Add(sArg)
                        sArg = ""
                    Else
                        sArg &= sArgs(i)
                    End If
                Case "("c, "["c
                    iLevel += 1
                    sArg &= sArgs(i)
                Case ")"c, "]"c
                    iLevel -= 1
                    sArg &= sArgs(i)
                Case Else
                    sArg &= sArgs(i)
            End Select
        Next
        If sArg <> "" Then lArgs.Add(sArg)

        Return lArgs
    End Function


    Private Sub EvaluateUDF(ByVal udf As clsUserFunction, ByRef sText As String)
        ' This will need to be a bit more sophisticated once we have arguments...
        Dim re As New System.Text.RegularExpressions.Regex("%" & udf.Name & "(\[.*?\])?%")

        If re.IsMatch(sText) Then
            ' We can't test udf.Output.ToString until we've replaced the parameters below

            ' Test for recursion
            For Each d As SingleDescription In udf.Output
                If d.Description.Contains("%" & udf.Name & If(udf.Arguments.Count = 0, "%", "[")) Then
                    ErrMsg("Recursive User Defined Function - " & udf.Name)
                    Exit Sub
                End If
            Next

            ' Replace each parameter with it's resolved value
            Dim sMatch As String = re.Match(sText).Value
            Dim dOut As Description = udf.Output.Copy

            ' Backup existing Refs
            Dim refsCopy As clsNewReference() = UserSession.NewReferences
            Dim refsUDF As clsNewReference() = {}
            Dim iRefNo As Integer = 0

            If sMatch.Contains("[") AndAlso sMatch.Contains("]") Then
                Dim sArgs As String = sMatch.Substring(sMatch.IndexOf("["c) + 1, sMatch.LastIndexOf("]"c) - sMatch.IndexOf("["c) - 1)

                Dim sArg As List(Of String) = SplitArgs(sArgs)
                Dim i As Integer = 0
                For Each arg As clsUserFunction.Argument In udf.Arguments
                    Dim sEvaluatedArg As String = ReplaceFunctions(sArg(i))

                    If sEvaluatedArg.Contains("|") Then ' Means it evaluated to multiple items                            
                        ' Depending on arg type, create an objects parameter, and set the refs
                        Select Case arg.Type
                            Case clsUserFunction.ArgumentType.Object
                                Dim refOb As New clsNewReference(ReferencesType.Object)
                                With refOb
                                    For Each sOb As String In sEvaluatedArg.Split("|"c)
                                        Dim itmOb As New clsSingleItem
                                        itmOb.MatchingPossibilities.Add(sOb)
                                        itmOb.bExplicitlyMentioned = True
                                        .Items.Add(itmOb)
                                    Next
                                End With
                                'sEvaluatedArg = "%objects%"
                                ReDim Preserve refsUDF(iRefNo)
                                refsUDF(iRefNo) = refOb
                                iRefNo += 1
                        End Select
                    End If

                    ' Our function argument could be an expression
                    If New System.Text.RegularExpressions.Regex("\d( )*[+-/*^]( )*\d").IsMatch(sEvaluatedArg) Then
                        Dim sExpr As String = EvaluateExpression(sEvaluatedArg)
                        If sExpr IsNot Nothing Then sEvaluatedArg = sExpr
                    End If

                    For Each d As SingleDescription In dOut
                        d.Description = d.Description.Replace("%" & arg.Name & "%", sEvaluatedArg)
                        For Each r As clsRestriction In d.Restrictions
                            If r.sKey1 = "Parameter-" & arg.Name Then r.sKey1 = If(sEvaluatedArg.Contains("|"), "ReferencedObjects", sEvaluatedArg).ToString
                            If r.sKey2 = "Parameter-" & arg.Name Then r.sKey2 = If(sEvaluatedArg.Contains("|"), "ReferencedObjects", sEvaluatedArg).ToString
                        Next
                    Next
                    i += 1
                Next
            End If

            UserSession.NewReferences = refsUDF
            Dim sFunctionResult As String = dOut.ToString
            ' Restore Refs
            UserSession.NewReferences = refsCopy
            sText = ReplaceFunctions(re.Replace(sText, sFunctionResult, 1))
        End If

    End Sub

    Public Function ReplaceFunctions(ByVal sText As String, Optional ByVal bExpression As Boolean = False, Optional ByVal bAllowOO As Boolean = True) As String
        Try
            Dim dictGUIDs As New Dictionary(Of String, String)
            While sText.Contains("<#")
                Dim iStart As Integer = sText.IndexOf("<#")
                Dim iEnd As Integer = sText.IndexOf("#>", iStart)
                Dim sGUID As String = ":" & System.Guid.NewGuid.ToString & ":"
                Dim sExpression As String = sText.Substring(iStart, iEnd - iStart + 2)
                dictGUIDs.Add(sGUID, sExpression)
                sText = sText.Replace(sExpression, sGUID)
            End While

            For Each udf As clsUserFunction In Adventure.htblUDFs.Values
                EvaluateUDF(udf, sText)
            Next

            If sInstr(sText, "%") > 0 Then
                Dim sCheck As String
                Do
                    sCheck = sText

                    sText = ReplaceIgnoreCase(sText, "%Player%", Adventure.Player.Key)

                    sText = ReplaceIgnoreCase(sText, "%object%", "%object1%")
                    sText = ReplaceIgnoreCase(sText, "%character%", "%character1%")
                    sText = ReplaceIgnoreCase(sText, "%location%", "%location1%")
                    sText = ReplaceIgnoreCase(sText, "%direction%", "%direction1%")
                    sText = ReplaceIgnoreCase(sText, "%item%", "%item1%")
                    sText = ReplaceIgnoreCase(sText, "%text%", "%text1%")
                    sText = ReplaceIgnoreCase(sText, "%number%", "%number1%")

                    sText = ReplaceIgnoreCase(sText, "%ConvCharacter%", Adventure.sConversationCharKey)
                    sText = ReplaceIgnoreCase(sText, "%turns%", Adventure.Turns.ToString)
                    Dim sVersion As String = System.Reflection.Assembly.GetExecutingAssembly.GetName.Version.ToString
                    sText = ReplaceIgnoreCase(sText, "%version%", sVersion.Substring(0, 1) & sVersion.Substring(2, 1) & sVersion.Substring(4))
                    sText = ReplaceIgnoreCase(sText, "%release%", Adventure.BabelTreatyInfo.Stories(0).Releases.Attached.Release.Version.ToString)

                    If Contains(sText, "%AloneWithChar%") Then
                        Dim sAloneWithChar As String = Adventure.Player.AloneWithChar
                        If sAloneWithChar Is Nothing Then sAloneWithChar = NOCHARACTER
                        sText = ReplaceIgnoreCase(sText, "%AloneWithChar%", sAloneWithChar)
                    End If
                    If sText.Contains("%CharacterName[") Then
                        For Each sPronoun As String In New String() {"subject", "subjective", "personal", "target", "object", "objective", "possessive"}
                            sText = ReplaceIgnoreCase(sText, "%CharacterName[" & sPronoun & "]%", "%CharacterName[%Player%, " & sPronoun & "]%")
                        Next
                    End If
                    sText = ReplaceIgnoreCase(sText, "%CharacterName%", "%CharacterName[%Player%]%") ' Function without args points to Player

                    With UserSession
                        If .NewReferences IsNot Nothing Then
                            For iObRef As Integer = 1 To 5
                                If Contains(sText, "%object" & iObRef & "%") Then
                                    ' Get the first object reference
                                    Dim bQuote As Boolean = bExpression AndAlso Not Contains(sText, "%object" & iObRef & "%.") AndAlso Not Contains(sText, """%object" & iObRef & "%""")
                                    For Each nr As clsNewReference In .NewReferences
                                        If nr.ReferenceType = ReferencesType.Object Then
                                            If nr.ReferenceMatch = "object" & iObRef Then
                                                Dim sObjects As String = ""
                                                For Each itm As clsSingleItem In nr.Items
                                                    If sObjects <> "" Then sObjects &= "|"
                                                    sObjects &= itm.MatchingPossibilities(0)
                                                Next
                                                If bQuote Then sObjects = """" & sObjects & """"
                                                If sObjects <> "" Then sText = ReplaceIgnoreCase(sText, "%object" & iObRef & "%", sObjects)
                                                Exit For
                                            End If
                                        End If
                                    Next
                                End If
                            Next iObRef

                            If Contains(sText, "%objects%") Then
                                ' Get the first object reference            
                                Dim bQuote As Boolean = bExpression AndAlso Not Contains(sText, "%objects%.") AndAlso Not Contains(sText, """%objects%""")
                                For Each nr As clsNewReference In .NewReferences
                                    If nr.ReferenceType = ReferencesType.Object AndAlso nr.ReferenceMatch = "objects" Then
                                        Dim sObjects As String = ""
                                        For Each itm As clsSingleItem In nr.Items
                                            If sObjects <> "" Then sObjects &= "|"
                                            sObjects &= itm.MatchingPossibilities(0)
                                        Next
                                        If bQuote Then sObjects = """" & sObjects & """"
                                        If sObjects <> "" Then sText = ReplaceIgnoreCase(sText, "%objects%", sObjects)
                                        Exit For
                                    End If
                                Next
                            End If

                            For iCharRef As Integer = 1 To 5
                                If Contains(sText, "%character" & iCharRef & "%") Then
                                    ' Get the first character reference       
                                    Dim bQuote As Boolean = bExpression AndAlso Not Contains(sText, "%character" & iCharRef & "%.") AndAlso Not Contains(sText, """%character" & iCharRef & "%""")
                                    For Each nr As clsNewReference In .NewReferences
                                        If nr.ReferenceType = ReferencesType.Character Then
                                            If nr.ReferenceMatch = "character" & iCharRef Then
                                                Dim sCharacters As String = ""
                                                For Each itm As clsSingleItem In nr.Items
                                                    If sCharacters <> "" Then sCharacters &= "|"
                                                    sCharacters &= itm.MatchingPossibilities(0)
                                                Next
                                                If bQuote Then sCharacters = """" & sCharacters & """"
                                                If sCharacters <> "" Then sText = ReplaceIgnoreCase(sText, "%character" & iCharRef & "%", sCharacters)
                                                Exit For
                                            End If
                                        End If
                                    Next
                                End If
                            Next iCharRef

                            If Contains(sText, "%characters%") Then
                                ' Get the first character reference 
                                Dim bQuote As Boolean = bExpression AndAlso Not Contains(sText, "%characters%.") AndAlso Not Contains(sText, """%characters%""")
                                For Each nr As clsNewReference In .NewReferences
                                    If nr.ReferenceType = ReferencesType.Character Then
                                        Dim sCharacters As String = ""
                                        For Each itm As clsSingleItem In nr.Items
                                            If sCharacters <> "" Then sCharacters &= "|"
                                            sCharacters &= itm.MatchingPossibilities(0)
                                        Next
                                        If bQuote Then sCharacters = """" & sCharacters & """"
                                        If sCharacters <> "" Then sText = ReplaceIgnoreCase(sText, "%characters%", sCharacters)
                                        Exit For
                                    End If
                                Next
                            End If

                            For iLocRef As Integer = 1 To 5
                                If Contains(sText, "%location" & iLocRef & "%") Then
                                    ' Get the first location reference                                           
                                    Dim bQuote As Boolean = bExpression AndAlso Not Contains(sText, "%location" & iLocRef & "%.") AndAlso Not Contains(sText, """%location" & iLocRef & "%""")
                                    For Each nr As clsNewReference In .NewReferences
                                        If nr.ReferenceType = ReferencesType.Location Then
                                            If nr.ReferenceMatch = "location" & iLocRef Then
                                                Dim sLocations As String = ""
                                                For Each itm As clsSingleItem In nr.Items
                                                    If sLocations <> "" Then sLocations &= "|"
                                                    sLocations &= itm.MatchingPossibilities(0)
                                                Next
                                                If bQuote Then sLocations = """" & sLocations & """"
                                                If sLocations <> "" Then sText = ReplaceIgnoreCase(sText, "%location" & iLocRef & "%", sLocations)
                                                Exit For
                                            End If
                                        End If
                                    Next
                                End If
                            Next iLocRef

                            For iItemRef As Integer = 1 To 5
                                If Contains(sText, "%item" & iItemRef & "%") Then
                                    ' Get the first item reference       
                                    Dim bQuote As Boolean = bExpression AndAlso Not Contains(sText, "%item" & iItemRef & "%.") AndAlso Not Contains(sText, """%item" & iItemRef & "%""")
                                    For Each nr As clsNewReference In .NewReferences
                                        If nr.ReferenceType = ReferencesType.Item Then
                                            If nr.ReferenceMatch = "item" & iItemRef Then
                                                Dim sItems As String = ""
                                                For Each itm As clsSingleItem In nr.Items
                                                    If sItems <> "" Then sItems &= "|"
                                                    sItems &= itm.MatchingPossibilities(0)
                                                Next
                                                If bQuote Then sItems = """" & sItems & """"
                                                If sItems <> "" Then sText = ReplaceIgnoreCase(sText, "%item" & iItemRef & "%", sItems)
                                                Exit For
                                            End If
                                        End If
                                    Next
                                End If
                            Next iItemRef

                            For iDirRef As Integer = 1 To 5
                                If Contains(sText, "%direction" & iDirRef & "%") Then
                                    ' Get the first direction reference       
                                    Dim bQuote As Boolean = bExpression AndAlso Not Contains(sText, "%direction" & iDirRef & "%.") AndAlso Not Contains(sText, """%direction" & iDirRef & "%""")
                                    For Each nr As clsNewReference In .NewReferences
                                        If nr.ReferenceType = ReferencesType.Direction Then
                                            If nr.ReferenceMatch = "direction" & iDirRef Then
                                                Dim sDirections As String = ""
                                                For Each itm As clsSingleItem In nr.Items
                                                    If sDirections <> "" Then sDirections &= "|"
                                                    sDirections &= itm.MatchingPossibilities(0)
                                                Next
                                                If bQuote Then sDirections = """" & sDirections & """"
                                                If sDirections <> "" Then sText = ReplaceIgnoreCase(sText, "%direction" & iDirRef & "%", sDirections)
                                                Exit For
                                            End If
                                        End If
                                    Next
                                End If
                            Next iDirRef

                            For i As Integer = 1 To 5
                                Dim iRef As Integer = i

                                Dim sNumber As String = "%number" & If(i > 0, i.ToString, "").ToString & "%"
                                If Contains(sText, sNumber) Then
                                    Dim bQuote As Boolean = bExpression AndAlso Not Contains(sText, "%number" & If(i > 0, i.ToString, "").ToString & "%.") AndAlso Not Contains(sText, """%number" & If(i > 0, i.ToString, "").ToString & "%""")
                                    For Each ref As clsNewReference In .NewReferences
                                        If ref.ReferenceType = ReferencesType.Number Then
                                            If ref.ReferenceMatch = "number" & iRef Then
                                                Dim sNumbers As String = ""
                                                For Each itm As clsSingleItem In ref.Items
                                                    If sNumbers <> "" Then sNumbers &= "|"
                                                    sNumbers &= itm.MatchingPossibilities(0)
                                                Next
                                                If bQuote Then sNumbers = """" & sNumbers & """"
                                                If sNumbers <> "" Then sText = ReplaceIgnoreCase(sText, sNumber, sNumbers)
                                                Exit For
                                            End If
                                        End If
                                    Next
                                End If

                                Dim sRefText As String = "%text" & If(i > 0, i.ToString, "").ToString & "%"
                                If Contains(sText, sRefText) Then
                                    Dim bQuote As Boolean = bExpression AndAlso Not Contains(sText, "%text" & If(i > 0, i.ToString, "").ToString & "%.") AndAlso Not Contains(sText, """%text" & If(i > 0, i.ToString, "").ToString & "%""")
                                    For Each ref As clsNewReference In .NewReferences
                                        If ref.ReferenceType = ReferencesType.Text Then
                                            If ref.ReferenceMatch = "text" & iRef Then
                                                If ref.Items.Count = 1 AndAlso ref.Items(0).MatchingPossibilities.Count = 1 Then
                                                    If bExpression Then
                                                        sText = ReplaceIgnoreCase(sText, sRefText, """" & ref.Items(0).MatchingPossibilities(0) & """")
                                                    Else
                                                        sText = ReplaceIgnoreCase(sText, sRefText, ref.Items(0).MatchingPossibilities(0))
                                                    End If
                                                End If
                                            End If
                                        End If
                                    Next
                                    If bQuote Then
                                        sText = ReplaceIgnoreCase(sText, sRefText, """" & Adventure.sReferencedText(iRef) & """")
                                    Else
                                        sText = ReplaceIgnoreCase(sText, sRefText, Adventure.sReferencedText(iRef))
                                    End If
                                End If
                            Next

                        End If
                    End With

                    For Each var As clsVariable In Adventure.htblVariables.Values
                        If var.Length = 1 Then
                            While InStr(sText, "%" & var.Name & "%", CompareMethod.Text) > 0
                                If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                                    sText = ReplaceIgnoreCase(sText, "%" & var.Name & "%", var.IntValue.ToString)
                                Else
                                    If bExpression Then
                                        sText = ReplaceIgnoreCase(sText, "%" & var.Name & "%", """" & var.StringValue & """")
                                    Else
                                        sText = ReplaceIgnoreCase(sText, "%" & var.Name & "%", var.StringValue)
                                    End If
                                End If
                            End While
                        ElseIf var.Length > 1 Then
                            While sInstr(sText, "%" & var.Name & "[") > 0
                                Dim iOffset As Integer = sInstr(sText, "%" & var.Name & "[") + var.Name.Length + 1
                                Dim sIndex As String = sText.Substring(iOffset, sText.IndexOf("]"c, iOffset) - iOffset)
                                If IsNumeric(sIndex) Then
                                    If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                                        sText = sText.Replace("%" & var.Name & "[" & sIndex & "]%", var.IntValue(CInt(sIndex)).ToString)
                                    Else
                                        If bExpression Then
                                            sText = sText.Replace("%" & var.Name & "[" & sIndex & "]%", """" & var.StringValue(CInt(sIndex)).ToString & """")
                                        Else
                                            sText = sText.Replace("%" & var.Name & "[" & sIndex & "]%", var.StringValue(CInt(sIndex)).ToString)
                                        End If
                                    End If
                                Else
                                    Dim iIndex As Integer = 0
                                    If IsNumeric(sIndex) Then
                                        iIndex = SafeInt(sIndex)
                                    Else
                                        Dim varIndex As New clsVariable
                                        varIndex.Type = clsVariable.VariableTypeEnum.Numeric
                                        varIndex.SetToExpression(sIndex)
                                        iIndex = varIndex.IntValue
                                    End If
                                    If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                                        sText = sText.Replace("%" & var.Name & "[" & sIndex & "]%", var.IntValue(iIndex).ToString)
                                    Else
                                        If bExpression Then
                                            sText = sText.Replace("%" & var.Name & "[" & sIndex & "]%", """" & var.StringValue(iIndex).ToString & """")
                                        Else
                                            sText = sText.Replace("%" & var.Name & "[" & sIndex & "]%", var.StringValue(iIndex).ToString)
                                        End If
                                    End If
                                End If
                            End While
                        End If
                    Next

                    ' Need this to evaluate text in order, not replace by function order
                    ' This is in case eg. "%DisplayCharacter% %CharacterName%" where DisplayCharacter contains name, then we must evaluate first 
                    ' name first in case we abbreviate, else we'll abbreviate the first name with 'he' instead of the second.
                    '
                    Dim sFirstFunction As String = ""
                    Dim iFirstLocation As Integer = Integer.MaxValue
                    For Each sFunctionCheck As String In FunctionNames()
                        sFunctionCheck = sFunctionCheck.ToLower
                        Dim iMatchCheck As Integer = sInstr(sText.ToLower, "%" & sFunctionCheck & "[")
                        If iMatchCheck > 0 AndAlso iMatchCheck < iFirstLocation Then
                            sFirstFunction = sFunctionCheck
                            iFirstLocation = iMatchCheck
                        End If
                    Next

                    If sFirstFunction <> "" Then
                        Dim sFunction As String = sFirstFunction
                        Dim iMatchLoc As Integer = sInstr(sText.ToLower, "%" & sFunction & "[")
                        While iMatchLoc > 0
                            Dim sArgs As String = GetFunctionArgs(sText.Substring(InStr(sText.ToLower, "%" & sFunction & "[") + sFunction.Length))
                            Dim iArgsLength As Integer = sArgs.Length
                            Dim bFunctionIsArgument As Boolean = (iMatchLoc > 1 AndAlso sText.Substring(iMatchLoc - 2, 1) = "[" AndAlso sText.Substring(iMatchLoc + sFunction.Length + iArgsLength, 2) = "]%")

                            If iArgsLength > 0 OrElse sFunction.ToLower = "sum" Then
                                Dim sOldArgs As String = sArgs
                                sArgs = ReplaceFunctions(sArgs)
                                sText = sText.Substring(0, iMatchLoc - 1) & Replace(sText, sOldArgs, sArgs, iMatchLoc, 1)

                                If sInstr(sText.ToLower, "%" & sFunction & "[" & sArgs.ToLower & "]%") > 0 Then
                                    Dim bAllowBlank As Boolean = False
                                    Dim sResult As String = ""
                                    Dim htblObjects As New ObjectHashTable
                                    Dim htblCharacters As New CharacterHashTable
                                    Dim htblLocations As New LocationHashTable

                                    If sArgs.Contains("|") Then
                                        ' We've got a pipe-separated list of parent keys                                    
                                        Dim sKeys() As String = sArgs.Split("|"c)
                                        For Each sKey As String In sKeys
                                            If sKey.Contains(",") Then sKey = sLeft(sKey, sInstr(sKey, ",") - 1) ' trim off any other args
                                            If Not htblObjects.ContainsKey(sKey) AndAlso Adventure.htblObjects.ContainsKey(sKey) Then htblObjects.Add(Adventure.htblObjects(sKey), sKey)
                                            If Not htblCharacters.ContainsKey(sKey) AndAlso Adventure.htblCharacters.ContainsKey(sKey) Then htblCharacters.Add(Adventure.htblCharacters(sKey), sKey)
                                            If Not htblLocations.ContainsKey(sKey) AndAlso Adventure.htblLocations.ContainsKey(sKey) Then htblLocations.Add(Adventure.htblLocations(sKey), sKey)
                                        Next
                                    Else
                                        Dim sKey As String = sArgs
                                        If sKey.Contains(",") Then sKey = sLeft(sKey, sInstr(sKey, ",") - 1)
                                        If Not htblObjects.ContainsKey(sKey) AndAlso Adventure.htblObjects.ContainsKey(sKey) Then htblObjects.Add(Adventure.htblObjects(sKey), sKey)
                                        If Not htblCharacters.ContainsKey(sKey) AndAlso Adventure.htblCharacters.ContainsKey(sKey) Then htblCharacters.Add(Adventure.htblCharacters(sKey), sKey)
                                        If Not htblLocations.ContainsKey(sKey) AndAlso Adventure.htblLocations.ContainsKey(sKey) Then htblLocations.Add(Adventure.htblLocations(sKey), sKey)
                                    End If

                                    Select Case sFunction
                                        Case "CharacterDescriptor".ToLower
                                            If Adventure.htblCharacters.ContainsKey(sArgs) Then
                                                ' This needs to be The, depending whether we have referred to them already... :-/
                                                sResult = htblCharacters(sArgs).Descriptor
                                            Else
                                                DisplayError("Bad Argument to &perc;CharacterDescriptor[]&perc; - Character Key """ & sArgs & """ not found")
                                            End If

                                        Case "CharacterName".ToLower
                                            Dim sKeys() As String = Split(sArgs.Replace(" ", ""), ",")
                                            Dim sKey As String = ""
                                            If sKeys.Length > 0 Then sKey = sKeys(0)
                                            Dim ePronoun As PronounEnum = PronounEnum.Subjective
                                            If sKeys.Length = 2 Then
                                                Select Case sKeys(1).ToLower
                                                    Case "subject", "subjective", "personal"
                                                        ePronoun = PronounEnum.Subjective
                                                    Case "target", "object", "objective"
                                                        ePronoun = PronounEnum.Objective
                                                    Case "possess", "possessive"
                                                        ePronoun = PronounEnum.Possessive
                                                    Case "none"
                                                        ePronoun = PronounEnum.None
                                                End Select
                                            End If

                                            If htblCharacters.ContainsKey(sKey) Then
                                                sResult = htblCharacters(sKey).Name(ePronoun)
                                                ' Slight fudge - if the name is the start of a sentence, auto-cap it... (consistent with v4)
                                                If iMatchLoc > 3 Then
                                                    If sText.Substring(iMatchLoc - 3, 2) = vbCrLf OrElse sText.Substring(iMatchLoc - 3, 2) = "  " Then
                                                        sResult = PCase(sResult)
                                                    End If
                                                End If
                                                If UserSession.bDisplaying Then UserSession.PronounKeys.Add(sKey, ePronoun, htblCharacters(sKey).Gender, UserSession.sOutputText.Length + iMatchLoc)
                                            ElseIf sKey = NOCHARACTER Then
                                                sResult = "Nobody"
                                            Else
                                                DisplayError("Bad Argument to &perc;CharacterName[]&perc; - Character Key """ & sArgs & """ not found")
                                            End If

                                        Case "CharacterProper".ToLower, "ProperName".ToLower
                                            If Adventure.htblCharacters.ContainsKey(sArgs) Then
                                                sResult = htblCharacters(sArgs).ProperName
                                            Else
                                                DisplayError("Bad Argument to &perc;CharacterProper[]&perc; - Character Key """ & sArgs & """ not found")
                                            End If

                                        Case "DisplayCharacter".ToLower
                                            If Adventure.htblCharacters.ContainsKey(sArgs) Then
                                                sResult = htblCharacters(sArgs).Description.ToString
                                                If sResult = "" Then sResult = "%CharacterName% see[//s] nothing interesting about %CharacterName[" & sArgs & ", target]%."
                                            Else
                                                DisplayError("Bad Argument to &perc;DisplayCharacter[]&perc; - Character Key """ & sArgs & """ not found")
                                            End If

                                        Case "DisplayLocation".ToLower
                                            If Adventure.htblLocations.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblLocations(sArgs).ViewLocation
                                                If sResult = "" Then sResult = "There is nothing of interest here."
                                            Else
                                                DisplayError("Bad Argument to &perc;DisplayLocation[]&perc; - Location Key """ & sArgs & """ not found")
                                            End If

                                        Case "DisplayObject".ToLower
                                            If Adventure.htblObjects.ContainsKey(sArgs) Then
                                                sResult = htblObjects(sArgs).Description.ToString
                                                If sResult = "" Then sResult = "%CharacterName% see[//s] nothing interesting about " & htblObjects(sArgs).FullName(ArticleTypeEnum.Definite) & "."
                                            Else
                                                DisplayError("Bad Argument to &perc;DisplayObject[]&perc; - Object Key """ & sArgs & """ not found")
                                            End If

                                        Case "Held".ToLower
                                            If Adventure.htblCharacters.ContainsKey(sArgs) Then
                                                For Each ob As clsObject In Adventure.htblCharacters(sArgs).HeldObjects.Values
                                                    If sResult <> "" Then sResult &= "|"
                                                    sResult &= ob.Key
                                                Next
                                            Else
                                                DisplayError("Bad Argument to &perc;Held[]&perc; - Character Key """ & sArgs & """ not found")
                                            End If
                                            bAllowBlank = True
                                        Case "LCase".ToLower
                                            sResult = sArgs.ToLower

                                        Case "ListCharactersOn".ToLower
                                            If Adventure.htblObjects.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblObjects(sArgs).ChildrenCharacters(clsObject.WhereChildrenEnum.OnObject).List()
                                            Else
                                                DisplayError("Bad Argument to &perc;ListCharactersOn[]&perc; - Object Key """ & sArgs & """ not found")
                                            End If

                                        Case "ListCharactersIn".ToLower
                                            If Adventure.htblObjects.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblObjects(sArgs).ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).List()
                                            Else
                                                DisplayError("Bad Argument to &perc;ListCharactersIn[]&perc; - Object Key """ & sArgs & """ not found")
                                            End If

                                        Case "ListCharactersOnAndIn".ToLower
                                            If Adventure.htblObjects.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblObjects(sArgs).DisplayCharacterChildren
                                            Else
                                                DisplayError("Bad Argument to &perc;ListCharactersOnAndIn[]&perc; - Object Key """ & sArgs & """ not found")
                                            End If

                                        Case "ListHeld".ToLower
                                            If Adventure.htblCharacters.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblCharacters(sArgs).HeldObjects.List(, True, ArticleTypeEnum.Indefinite)
                                            Else
                                                DisplayError("Bad Argument to &perc;ListHeld[]&perc; - Character Key """ & sArgs & """ not found")
                                            End If
                                        Case "ListExits".ToLower
                                            If Adventure.htblCharacters.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblCharacters(sArgs).ListExits
                                            Else
                                                DisplayError("Bad Argument to &perc;ListExits[]&perc; - Character Key """ & sArgs & """ not found")
                                            End If
                                        Case "ListObjectsAtLocation".ToLower
                                            If Adventure.htblLocations.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblLocations(sArgs).ObjectsInLocation(clsLocation.WhichObjectsToListEnum.AllObjects, True).List(, , ArticleTypeEnum.Indefinite)
                                            Else
                                                DisplayError("Bad Argument to &perc;ListObjectsAtLocation[]&perc; - Location Key """ & sArgs & """ not found")
                                            End If

                                        Case "ListObjectsOn".ToLower
                                            If Adventure.htblObjects.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblObjects(sArgs).Children(clsObject.WhereChildrenEnum.OnObject).List(, , ArticleTypeEnum.Indefinite)
                                            Else
                                                DisplayError("Bad Argument to &perc;ListObjectsOn[]&perc; - Object Key """ & sArgs & """ not found")
                                            End If

                                        Case "ListObjectsIn".ToLower
                                            If Adventure.htblObjects.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblObjects(sArgs).Children(clsObject.WhereChildrenEnum.InsideObject).List(, , ArticleTypeEnum.Indefinite)
                                            Else
                                                DisplayError("Bad Argument to &perc;ListObjectsIn[]&perc; - Object Key """ & sArgs & """ not found")
                                            End If

                                        Case "ListObjectsOnAndIn".ToLower
                                            If htblObjects.Count > 0 Then
                                                For Each ob As clsObject In htblObjects.Values
                                                    If htblObjects.Count = 1 OrElse ob.Children(clsObject.WhereChildrenEnum.InsideOrOnObject).Count > 0 Then
                                                        If sResult <> "" Then pSpace(sResult)
                                                        sResult &= ob.DisplayObjectChildren
                                                    End If
                                                Next
                                            Else
                                                DisplayError("Bad Argument to &perc;ListObjectsOnAndIn[]&perc; - Object Key """ & sArgs & """ not found")
                                            End If
                                        Case "ListWorn".ToLower
                                            If Adventure.htblCharacters.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblCharacters(sArgs).WornObjects.List("and", True, ArticleTypeEnum.Indefinite)
                                            Else
                                                DisplayError("Bad Argument to &perc;ListWorn[]&perc; - Character Key """ & sArgs & """ not found")
                                            End If
                                        Case "LocationName".ToLower
                                            If Adventure.htblLocations.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblLocations(sArgs).ShortDescription.ToString
                                            Else
                                                DisplayError("Bad Argument to &perc;LocationName[]&perc; - Location Key """ & sArgs & """ not found")
                                            End If

                                        Case "LocationOf".ToLower
                                            If Adventure.htblCharacters.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblCharacters(sArgs).Location.LocationKey
                                            ElseIf Adventure.htblObjects.ContainsKey(sArgs) Then
                                                For Each sLocKey As String In Adventure.htblObjects(sArgs).LocationRoots.Keys
                                                    If sResult <> "" Then sResult &= "|"
                                                    sResult &= sLocKey
                                                Next
                                            Else
                                                DisplayError("Bad Argument to &perc;LocationOf[]&perc; - Character Key """ & sArgs & """ not found")
                                            End If

                                        Case "NumberAsText".ToLower
                                            sResult = NumberToString(sArgs)

                                        Case "ObjectName".ToLower
                                            sResult = htblObjects.List(, , ArticleTypeEnum.Indefinite)

                                        Case "ObjectsIn".ToLower
                                            If Adventure.htblObjects.ContainsKey(sArgs) Then
                                                For Each ob As clsObject In Adventure.htblObjects(sArgs).Children(clsObject.WhereChildrenEnum.InsideObject).Values
                                                    If sResult <> "" Then sResult &= "|"
                                                    sResult &= ob.Key
                                                Next
                                            Else
                                                DisplayError("Bad Argument to &perc;ObjectsIn[]&perc; - Object Key """ & sArgs & """ not found")
                                            End If
                                            bAllowBlank = True

                                        Case "ParentOf".ToLower
                                            For Each ob As clsObject In htblObjects.Values
                                                If sResult <> "" Then sResult &= "|"
                                                sResult &= ob.Parent
                                            Next
                                            For Each ch As clsCharacter In htblCharacters.Values
                                                If sResult <> "" Then sResult &= "|"
                                                sResult &= ch.Parent
                                            Next
                                        Case "PCase".ToLower
                                            sResult = PCase(sArgs, , bExpression)

                                        Case "PopUpChoice".ToLower
                                            Dim sKeys() As String = sArgs.Split(","c) ' TODO - Improve this to read quotes and commas properly
                                            If sKeys.Length = 3 Then
                                                ' For now - needs improving
                                                Dim var As New clsVariable
                                                var.Type = clsVariable.VariableTypeEnum.Text
                                                Select Case MsgBox(sKeys(0) & vbCrLf & vbCrLf & "Yes for " & sKeys(1) & ", No for " & sKeys(2) & " (dialog box to be improved!)", MsgBoxStyle.YesNo)
                                                    Case MsgBoxResult.Yes
                                                        var.SetToExpression(sKeys(1))
                                                    Case MsgBoxResult.No
                                                        var.SetToExpression(sKeys(2))
                                                End Select
                                                sResult = var.StringValue
                                                If bExpression Then sResult = """" & sResult & """"
                                            Else
                                                DisplayError("Bad arguments to PopUpChoice function: PopUpChoice[prompt, choice1, choice2]")
                                            End If

                                        Case "PopUpInput".ToLower
                                            Dim sKeys() As String = sArgs.Split(","c) ' TODO - Improve this to read quotes and commas properly
                                            If sKeys.Length = 1 OrElse sKeys.Length = 2 Then
                                                Dim sDefault As String = ""
                                                If sKeys.Length = 2 Then sDefault = EvaluateExpression(sKeys(1))
                                                sResult = """" & InputBox(EvaluateExpression(sKeys(0)), "ADRIFT", sDefault) & """"
                                            Else
                                                DisplayError("Expecting 1 or two arguments to PopUpInput[prompt, default]")
                                            End If

                                        Case "PrevListObjectsOn".ToLower
                                            ' Maintain a 'last turn' state
                                            ' Call ListObjectsOn on this state
                                            sResult = PreviousFunction(sFunction, sArgs)

                                        Case "PrevParentOf".ToLower
                                            ' Get rid of PrevParent, and do the same as above
                                            For Each ob As clsObject In htblObjects.Values
                                                If sResult <> "" Then sResult &= "|"
                                                sResult &= ob.PrevParent
                                            Next
                                            For Each ch As clsCharacter In htblCharacters.Values
                                                If sResult <> "" Then sResult &= "|"
                                                sResult &= ch.PrevParent
                                            Next

                                        Case "PropertyValue".ToLower
                                            bAllowBlank = True
                                            Dim sKeys() As String = Split(sArgs.Replace(" ", ""), ",")
                                            If sKeys.Length = 2 Then
                                                If htblObjects.Count + htblCharacters.Count + htblLocations.Count > 0 Then
                                                    Dim arlOutput As New StringArrayList
                                                    For Each ob As clsObject In htblObjects.Values
                                                        If ob.HasProperty(sKeys(1)) Then
                                                            arlOutput.Add(ob.GetPropertyValue(sKeys(1)))
                                                        Else
                                                            DisplayError("Bad 2nd Argument to &perc;PropertyValue[]&perc; - Property Key """ & sKeys(1) & """ not found")
                                                        End If
                                                    Next
                                                    For Each ch As clsCharacter In htblCharacters.Values
                                                        If ch.HasProperty(sKeys(1)) Then
                                                            arlOutput.Add(ch.GetPropertyValue(sKeys(1)))
                                                        Else
                                                            DisplayError("Bad 2nd Argument to &perc;PropertyValue[]&perc; - Property Key """ & sKeys(1) & """ not found")
                                                        End If
                                                    Next
                                                    For Each l As clsLocation In htblLocations.Values
                                                        If l.HasProperty(sKeys(1)) Then
                                                            arlOutput.Add(l.GetPropertyValue(sKeys(1)))
                                                        Else
                                                            DisplayError("Bad 2nd Argument to &perc;PropertyValue[]&perc; - Property Key """ & sKeys(1) & """ not found")
                                                        End If
                                                    Next
                                                    sResult = arlOutput.List
                                                Else
                                                    ' Only warn about the first arg if it isn't from a function
                                                    Dim sOrig As String = Split(sOldArgs.Replace(" ", ""), ",")(0)
                                                    If sOrig = sKeys(0) Then DisplayError("Bad 1st Argument to &perc;PropertyValue[]&perc; - Object/Character Key """ & sKeys(0) & """ not found")
                                                End If
                                            Else
                                                DisplayError("Bad call to &perc;PropertyValue[]&perc; - Two arguments expected; Object Key, Property Key")
                                            End If

                                        Case "Sum".ToLower
                                            ' Sum the numbers from a string
                                            Dim sInput As New System.Text.StringBuilder
                                            For Each c As Char In sArgs.ToCharArray
                                                Select Case c
                                                    Case "0"c To "9"c, "-"c
                                                        sInput.Append(c)
                                                    Case Else
                                                        sInput.Append(" ")
                                                End Select
                                            Next
                                            While sInput.ToString.Contains("  ")
                                                sInput.Replace("  ", " ")
                                            End While
                                            Dim iTotal As Integer = 0
                                            For Each s As String In sInput.ToString.Split(" "c)
                                                iTotal += SafeInt(s)
                                            Next
                                            sResult = iTotal.ToString

                                        Case "TaskCompleted".ToLower
                                            If Adventure.htblTasks.ContainsKey(sArgs) Then
                                                sResult = Adventure.htblTasks(sArgs).Completed.ToString
                                            Else
                                                DisplayError("Bad Argument to &perc;TaskCompleted[]&perc; - Task Key """ & sArgs & """ not found")
                                            End If

                                        Case "TheObject".ToLower, "TheObjects".ToLower
                                            sResult = htblObjects.List

                                        Case "UCase".ToLower
                                            sResult = sArgs.ToUpper

                                        Case "Worn".ToLower
                                            If Adventure.htblCharacters.ContainsKey(sArgs) Then
                                                For Each ob As clsObject In Adventure.htblCharacters(sArgs).WornObjects.Values
                                                    If sResult <> "" Then sResult &= "|"
                                                    sResult &= ob.Key
                                                Next
                                            Else
                                                DisplayError("Bad Argument to &perc;Worn[]&perc; - Character Key """ & sArgs & """ not found")
                                            End If
                                    End Select

                                    If sResult = "" AndAlso Not bAllowBlank Then
                                        DisplayError("Bad Function - Nothing output")
                                    End If

                                    sText = ReplaceIgnoreCase(sText, "%" & sFunction & "[" & sArgs & "]%", sResult)

                                Else
                                    Dim sBadFunction As String = ""
                                    If sText.ToLower.Contains("%" & sFunction) Then sBadFunction = "%" & sFunction
                                    If sText.ToLower.Contains("%" & sFunction & "[") Then sBadFunction = "%" & sFunction & "["
                                    If sText.ToLower.Contains("%" & sFunction & "[" & sArgs.ToLower) Then sBadFunction = "%" & sFunction & "[" & sArgs
                                    If sText.ToLower.Contains("%" & sFunction & "[" & sArgs.ToLower & "]") Then sBadFunction = "%" & sFunction & "[" & sArgs & "]"
                                    sText = ReplaceIgnoreCase(sText, sBadFunction, " <c><u>" & sBadFunction.Replace("%", "&perc;") & "</u></c>")

                                    DisplayError("Bad function " & sFunction)

                                End If
                            Else
                                sText = ReplaceIgnoreCase(sText, "%" & sFunction & "[]%", "")
                                sText = ReplaceIgnoreCase(sText, "%" & sFunction & "[", "")

                                DisplayError("No arguments given to function " & sFunction)

                            End If

                            iMatchLoc = sInstr(sText.ToLower, "%" & sFunction & "[")
                        End While
                    End If

                Loop While sText <> sCheck
            End If


            If bAllowOO Then
                Dim sPrev As String
                Do
                    sPrev = sText
                    sText = ReplaceOO(sText, bExpression)
                Loop While sText <> sPrev
            End If


            Dim rePerspective As New System.Text.RegularExpressions.Regex("\[(?<first>.*?)/(?<second>.*?)/(?<third>.*?)\]")
            While rePerspective.IsMatch(sText)
                Dim match As System.Text.RegularExpressions.Match = rePerspective.Match(sText)
                Dim sFirst As String = match.Groups("first").Value
                If sFirst.Contains("[") Then sFirst = sFirst.Substring(sFirst.LastIndexOf("[") + 1)
                Dim sSecond As String = match.Groups("second").Value
                Dim sThird As String = match.Groups("third").Value
                Dim sValue As String = ""

                If sFirst.Contains("[") OrElse sFirst.Contains("]") OrElse sSecond.Contains("[") OrElse sSecond.Contains("]") OrElse sThird.Contains("[") OrElse sThird.Contains("]") Then Exit While

                Select Case GetPerspective(match.Index)
                    Case PerspectiveEnum.FirstPerson
                        sValue = sFirst
                    Case PerspectiveEnum.SecondPerson
                        sValue = sSecond
                    Case PerspectiveEnum.ThirdPerson
                        sValue = sThird
                End Select

                sText = sText.Replace("[" & sFirst & "/" & sSecond & "/" & sThird & "]", sValue)
            End While

            For Each sGUID As String In dictGUIDs.Keys
                sText = sText.Replace(sGUID, dictGUIDs(sGUID))
            Next

            Return sText

        Catch ex As Exception
            ErrMsg("ReplaceFunctions error", ex)
            Return sText
        End Try

    End Function
    Private Function GetPerspective(ByVal iOffset As Integer) As PerspectiveEnum

        Dim iHighest As Integer = 0
        Dim ePerspective As PerspectiveEnum = PerspectiveEnum.None

#If Runner Then
        For Each p As PronounInfo In UserSession.PronounKeys
            If iOffset >= p.Offset AndAlso p.Offset > iHighest Then
                ePerspective = Adventure.htblCharacters(p.Key).Perspective
                iHighest = p.Offset
            End If
        Next
#End If

        If ePerspective <> PerspectiveEnum.None Then
            Return ePerspective
        Else
            Return Adventure.Player.Perspective
        End If

    End Function


    ' Convert a number between 0 and 999 into words.
    Private Function GroupToWords(ByVal num As Integer) As _
        String
        Static one_to_nineteen() As String = {"zero", "one",
            "two", "three", "four", "five", "six", "seven",
            "eight", "nine", "ten", "eleven", "twelve",
            "thirteen", "fourteen", "fifteen", "sixteen",
            "seventeen", "eighteen", "nineteen"}
        Static multiples_of_ten() As String = {"twenty",
            "thirty", "forty", "fifty", "sixty", "seventy",
            "eighty", "ninety"}

        ' If the number is 0, return an empty string.
        If num = 0 Then Return ""

        ' Handle the hundreds digit.
        Dim digit As Integer
        Dim result As String = ""
        Dim bAnd As Boolean = False
        If num > 99 Then
            digit = num \ 100
            num = num Mod 100
            bAnd = True
            result = one_to_nineteen(digit) & " hundred"
        End If

        ' If num = 0, we have hundreds only.
        If num = 0 Then Return result.Trim()

        If bAnd Then result &= " and"

        ' See if the rest is less than 20.
        If num < 20 Then
            ' Look up the correct name.
            result &= " " & one_to_nineteen(num)
        Else
            ' Handle the tens digit.
            digit = num \ 10
            num = num Mod 10
            result &= " " & multiples_of_ten(digit - 2)

            ' Handle the final digit.
            If num > 0 Then
                result &= " " & one_to_nineteen(num)
            End If
        End If

        Return result.Trim()
    End Function

    ' Return a word representation of the whole number value.
    Private Function NumberToString(ByVal num_str As String, Optional ByVal use_us_group_names As Boolean = True) As String

        Dim result As String = ""
        Dim sIn As String = num_str

        If Not IsNumeric(num_str) Then
            Dim v As New clsVariable
            v.SetToExpression(num_str)
            num_str = v.IntValue.ToString
        End If

        Try

            ' Get the appropiate group names.
            Dim groups() As String
            If use_us_group_names Then
                groups = New String() {"", "thousand", "million",
                    "billion", "trillion", "quadrillion",
                    "quintillion", "sextillion", "septillion",
                    "octillion", "nonillion", "decillion",
                    "undecillion", "duodecillion", "tredecillion",
                    "quattuordecillion", "quindecillion",
                    "sexdecillion", "septendecillion",
                    "octodecillion", "novemdecillion",
                    "vigintillion"}
            Else
                groups = New String() {"", "thousand", "million",
                    "milliard", "billion", "1000 billion",
                    "trillion", "1000 trillion", "quadrillion",
                    "1000 quadrillion", "quintillion", "1000 " &
                    "quintillion", "sextillion", "1000 sextillion",
                    "septillion", "1000 septillion", "octillion",
                    "1000 octillion", "nonillion", "1000 " &
                    "nonillion", "decillion", "1000 decillion"}
            End If

            ' Clean the string a bit.
            ' Remove "$", ",", leading zeros, and
            ' anything after a decimal point.
            Const CURRENCY As String = "$"
            Const SEPARATOR As String = ","
            Const DECIMAL_POINT As String = "."
            num_str = num_str.Replace(CURRENCY,
                "").Replace(SEPARATOR, "")
            num_str = num_str.TrimStart(New Char() {"0"c})
            Dim pos As Integer = num_str.IndexOf(DECIMAL_POINT)
            If pos = 0 Then
                Return "zero"
            ElseIf pos > 0 Then
                num_str = num_str.Substring(0, pos - 1)
            End If

            ' See how many groups there will be.
            Dim num_groups As Integer = (num_str.Length + 2) \ 3

            ' Pad so length is a multiple of 3.
            num_str = num_str.PadLeft(num_groups * 3, " "c)

            ' Process the groups, largest first.            
            Dim group_num As Integer
            For group_num = num_groups - 1 To 0 Step -1
                ' Get the next three digits.
                Dim group_str As String = num_str.Substring(0, 3)
                num_str = num_str.Substring(3)
                Dim group_value As Integer = CInt(group_str)

                ' Convert the group into words.
                If group_value > 0 Then
                    If group_num >= groups.Length Then
                        result &= GroupToWords(group_value) &
                            " ?, "
                    Else
                        result &= GroupToWords(group_value) &
                            " " & groups(group_num) & ", "
                    End If
                End If
            Next group_num

            ' Remove the trailing ", ".
            If result.EndsWith(", ") Then
                result = result.Substring(0, result.Length - 2)
            End If

            result = result.Trim()

        Catch ex As Exception
            ErrMsg("NumberToString error parsing """ & sIn & """", ex)
        End Try

        If result = "" Then result = "zero"
        Return result

    End Function

    Public Function OppositeDirection(ByVal dir As DirectionsEnum) As DirectionsEnum
        Select Case dir
            Case DirectionsEnum.North
                Return DirectionsEnum.South
            Case DirectionsEnum.NorthEast
                Return DirectionsEnum.SouthWest
            Case DirectionsEnum.East
                Return DirectionsEnum.West
            Case DirectionsEnum.SouthEast
                Return DirectionsEnum.NorthWest
            Case DirectionsEnum.South
                Return DirectionsEnum.North
            Case DirectionsEnum.SouthWest
                Return DirectionsEnum.NorthEast
            Case DirectionsEnum.West
                Return DirectionsEnum.East
            Case DirectionsEnum.NorthWest
                Return DirectionsEnum.SouthEast
            Case DirectionsEnum.Up
                Return DirectionsEnum.Down
            Case DirectionsEnum.Down
                Return DirectionsEnum.Up
            Case DirectionsEnum.In
                Return DirectionsEnum.Out
            Case DirectionsEnum.Out
                Return DirectionsEnum.In
            Case Else
                Return dir
        End Select
    End Function

    Public Function ReplaceIgnoreCase(ByVal Expression As String, ByVal Find As String, ByVal Replacement As String) As String
        If Replacement Is Nothing Then Replacement = ""
        Dim regex As New System.Text.RegularExpressions.Regex(Find.Replace("[", "\[").Replace("]", "\]").Replace("(", "\(").Replace(")", "\)").Replace("|", "\|").Replace("*", "\*").Replace("?", "\?").Replace("$", "\$").Replace("^", "\^").Replace("+", "\+"), System.Text.RegularExpressions.RegexOptions.IgnoreCase)
        Return regex.Replace(Expression, Replacement.Replace("$", "$$"), 1) ' Prevent Substitutions (http://msdn.microsoft.com/en-us/library/ewy2t5e0.aspx)
    End Function

    Public Function PCase(ByVal sText As String, Optional ByVal bStrictLower As Boolean = False, Optional ByVal bExpression As Boolean = False) As String
        Dim bQuotes As Boolean = False
        If bExpression AndAlso sText.StartsWith("""") AndAlso sText.EndsWith("""") Then
            bQuotes = True
            sText = sText.Substring(1, sText.Length - 2)
        End If
        PCase = ""

        Try
            If sText.Length > 0 Then
                PCase = Left(sText, 1).ToUpper
                If sText.Length > 1 Then
                    If bStrictLower Then
                        PCase &= Right(sText, sText.Length - 1).ToLower
                    Else
                        PCase &= Right(sText, sText.Length - 1)
                    End If
                End If
            Else
                Return ""
            End If
        Finally
            If bQuotes Then PCase = """" & PCase & """"
        End Try
    End Function


    Public Function GetFunctionArgs(ByVal sText As String) As String
        Try
            If sInstr(sText, "[") = 0 OrElse sInstr(sText, "]") = 0 Then Return ""

            ' Work out this bracket chunk, then run ReplaceFunctions on it
            Dim iOffset As Integer = 1
            Dim iLevel As Integer = 1

            While iLevel > 0
                Select Case sText.Substring(iOffset, 1)
                    Case "["
                        iLevel += 1
                    Case "]"
                        iLevel -= 1
                    Case Else
                        ' Ignore
                End Select
                iOffset += 1
            End While

            Return sText.Substring(1, iOffset - 2)
        Catch ex As Exception
            DisplayError("Error obtaining function arguments from """ & sText & """")
            Return ""
        End Try
    End Function

    Public Function EnumParsePropertyPropertyOf(ByVal sValue As String) As clsProperty.PropertyOfEnum
        Select Case sValue
            Case "Objects"
                Return clsProperty.PropertyOfEnum.Objects
            Case "Characters"
                Return clsProperty.PropertyOfEnum.Characters
            Case "Locations"
                Return clsProperty.PropertyOfEnum.Locations
            Case "AnyItem"
                Return clsProperty.PropertyOfEnum.AnyItem
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParsePropertyType(ByVal sValue As String) As clsProperty.PropertyTypeEnum
        Select Case sValue
            Case "CharacterKey"
                Return clsProperty.PropertyTypeEnum.CharacterKey
            Case "Integer"
                Return clsProperty.PropertyTypeEnum.Integer
            Case "LocationGroupKey"
                Return clsProperty.PropertyTypeEnum.LocationGroupKey
            Case "LocationKey"
                Return clsProperty.PropertyTypeEnum.LocationKey
            Case "ObjectKey"
                Return clsProperty.PropertyTypeEnum.ObjectKey
            Case "SelectionOnly"
                Return clsProperty.PropertyTypeEnum.SelectionOnly
            Case "StateList"
                Return clsProperty.PropertyTypeEnum.StateList
            Case "ValueList"
                Return clsProperty.PropertyTypeEnum.ValueList
            Case "Text"
                Return clsProperty.PropertyTypeEnum.Text
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseMoveCharacter(ByVal sValue As String) As clsAction.MoveCharacterToEnum
        Select Case sValue
            Case "InDirection"
                Return clsAction.MoveCharacterToEnum.InDirection
            Case "ToLocation"
                Return clsAction.MoveCharacterToEnum.ToLocation
            Case "ToLocationGroup"
                Return clsAction.MoveCharacterToEnum.ToLocationGroup
            Case "ToLyingOn"
                Return clsAction.MoveCharacterToEnum.ToLyingOn
            Case "ToSameLocationAs"
                Return clsAction.MoveCharacterToEnum.ToSameLocationAs
            Case "ToSittingOn"
                Return clsAction.MoveCharacterToEnum.ToSittingOn
            Case "ToStandingOn"
                Return clsAction.MoveCharacterToEnum.ToStandingOn
            Case "ToSwitchWith"
                Return clsAction.MoveCharacterToEnum.ToSwitchWith
            Case "InsideObject"
                Return clsAction.MoveCharacterToEnum.InsideObject
            Case "OntoCharacter"
                Return clsAction.MoveCharacterToEnum.OntoCharacter
            Case "ToParentLocation"
                Return clsAction.MoveCharacterToEnum.ToParentLocation
            Case "ToGroup"
                Return clsAction.MoveCharacterToEnum.ToGroup
            Case "FromGroup"
                Return clsAction.MoveCharacterToEnum.FromGroup
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseMoveLocation(ByVal sValue As String) As clsAction.MoveLocationToEnum
        Select Case sValue
            Case "ToGroup"
                Return clsAction.MoveLocationToEnum.ToGroup
            Case "FromGroup"
                Return clsAction.MoveLocationToEnum.FromGroup
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseSetTask(ByVal sValue As String) As clsAction.SetTasksEnum
        Select Case sValue
            Case "Execute"
                Return clsAction.SetTasksEnum.Execute
            Case "Unset"
                Return clsAction.SetTasksEnum.Unset
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseEndGame(ByVal sValue As String) As clsAction.EndGameEnum
        Select Case sValue
            Case "Win"
                Return clsAction.EndGameEnum.Win
            Case "Lose"
                Return clsAction.EndGameEnum.Lose
            Case "Neutral"
                Return clsAction.EndGameEnum.Neutral
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseMoveObject(ByVal sValue As String) As clsAction.MoveObjectToEnum
        Select Case sValue
            Case "InsideObject"
                Return clsAction.MoveObjectToEnum.InsideObject
            Case "OntoObject"
                Return clsAction.MoveObjectToEnum.OntoObject
            Case "ToCarriedBy"
                Return clsAction.MoveObjectToEnum.ToCarriedBy
            Case "ToLocation"
                Return clsAction.MoveObjectToEnum.ToLocation
            Case "ToLocationGroup"
                Return clsAction.MoveObjectToEnum.ToLocationGroup
            Case "ToPartOfCharacter"
                Return clsAction.MoveObjectToEnum.ToPartOfCharacter
            Case "ToPartOfObject"
                Return clsAction.MoveObjectToEnum.ToPartOfObject
            Case "ToSameLocationAs"
                Return clsAction.MoveObjectToEnum.ToSameLocationAs
            Case "ToWornBy"
                Return clsAction.MoveObjectToEnum.ToWornBy
            Case "ToGroup"
                Return clsAction.MoveObjectToEnum.ToGroup
            Case "FromGroup"
                Return clsAction.MoveObjectToEnum.FromGroup
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseMust(ByVal sValue As String) As clsRestriction.MustEnum
        Select Case sValue
            Case "Must"
                Return clsRestriction.MustEnum.Must
            Case "MustNot"
                Return clsRestriction.MustEnum.MustNot
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseLocation(ByVal sValue As String) As clsRestriction.LocationEnum
        If Adventure.dVersion < 5.000015 Then
            If sValue = "SeenByCharacter" Then sValue = "HaveBeenSeenByCharacter"
        End If
        Select Case sValue
            Case "BeInGroup"
                Return clsRestriction.LocationEnum.BeInGroup
            Case "HaveBeenSeenByCharacter"
                Return clsRestriction.LocationEnum.HaveBeenSeenByCharacter
            Case "HaveProperty"
                Return clsRestriction.LocationEnum.HaveProperty
            Case "BeLocation"
                Return clsRestriction.LocationEnum.BeLocation
            Case "Exist"
                Return clsRestriction.LocationEnum.Exist
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseDirections(ByVal sValue As String) As DirectionsEnum
        Select Case sValue
            Case "Down"
                Return DirectionsEnum.Down
            Case "East"
                Return DirectionsEnum.East
            Case "In"
                Return DirectionsEnum.In
            Case "North"
                Return DirectionsEnum.North
            Case "NorthEast"
                Return DirectionsEnum.NorthEast
            Case "NorthWest"
                Return DirectionsEnum.NorthWest
            Case "Out"
                Return DirectionsEnum.Out
            Case "South"
                Return DirectionsEnum.South
            Case "SouthEast"
                Return DirectionsEnum.SouthEast
            Case "SouthWest"
                Return DirectionsEnum.SouthWest
            Case "Up"
                Return DirectionsEnum.Up
            Case "West"
                Return DirectionsEnum.West
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseBeforeAfter(ByVal sValue As String) As clsTask.BeforeAfterEnum
        Select Case sValue
            Case "After"
                Return clsTask.BeforeAfterEnum.After
            Case "Before"
                Return clsTask.BeforeAfterEnum.Before
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseVariable(ByVal sValue As String) As clsRestriction.VariableEnum
        Select Case sValue
            Case "EqualTo"
                Return clsRestriction.VariableEnum.EqualTo
            Case "GreaterThan"
                Return clsRestriction.VariableEnum.GreaterThan
            Case "GreaterThanOrEqualTo"
                Return clsRestriction.VariableEnum.GreaterThanOrEqualTo
            Case "LessThan"
                Return clsRestriction.VariableEnum.LessThan
            Case "LessThanOrEqualTo"
                Return clsRestriction.VariableEnum.LessThanOrEqualTo
            Case "Contain"
                Return clsRestriction.VariableEnum.Contain
            Case Else
                Return Nothing
        End Select
    End Function

    Public Function EnumParseItem(ByVal sValue As String) As clsRestriction.ItemEnum
        Select Case sValue
            Case "BeAtLocation"
                Return clsRestriction.ItemEnum.BeAtLocation
            Case "BeCharacter"
                Return clsRestriction.ItemEnum.BeCharacter
            Case "BeInGroup"
                Return clsRestriction.ItemEnum.BeInGroup
            Case "BeInSameLocationAsCharacter"
                Return clsRestriction.ItemEnum.BeInSameLocationAsCharacter
            Case "BeInSameLocationAsObject"
                Return clsRestriction.ItemEnum.BeInSameLocationAsObject
            Case "BeInsideObject"
                Return clsRestriction.ItemEnum.BeInsideObject
            Case "BeLocation"
                Return clsRestriction.ItemEnum.BeLocation
            Case "BeObject"
                Return clsRestriction.ItemEnum.BeObject
            Case "BeOnCharacter"
                Return clsRestriction.ItemEnum.BeOnCharacter
            Case "BeOnObject"
                Return clsRestriction.ItemEnum.BeOnObject
            Case "BeType"
                Return clsRestriction.ItemEnum.BeType
            Case "Exist"
                Return clsRestriction.ItemEnum.Exist
            Case "HaveProperty"
                Return clsRestriction.ItemEnum.HaveProperty
            Case Else
                TODO()
        End Select
    End Function

    Public Function EnumParseCharacter(ByVal sValue As String) As clsRestriction.CharacterEnum
        Select Case sValue
            Case "BeAlone"
                Return clsRestriction.CharacterEnum.BeAlone
            Case "BeAloneWith"
                Return clsRestriction.CharacterEnum.BeAloneWith
            Case "BeAtLocation"
                Return clsRestriction.CharacterEnum.BeAtLocation
            Case "BeCharacter"
                Return clsRestriction.CharacterEnum.BeCharacter
            Case "BeInConversationWith"
                Return clsRestriction.CharacterEnum.BeInConversationWith
            Case "Exist"
                Return clsRestriction.CharacterEnum.Exist
            Case "HaveRouteInDirection"
                Return clsRestriction.CharacterEnum.HaveRouteInDirection
            Case "HaveSeenCharacter"
                Return clsRestriction.CharacterEnum.HaveSeenCharacter
            Case "HaveSeenLocation"
                Return clsRestriction.CharacterEnum.HaveSeenLocation
            Case "HaveSeenObject"
                Return clsRestriction.CharacterEnum.HaveSeenObject
            Case "BeHoldingObject"
                Return clsRestriction.CharacterEnum.BeHoldingObject
            Case "BeInSameLocationAsCharacter"
                Return clsRestriction.CharacterEnum.BeInSameLocationAsCharacter
            Case "BeInSameLocationAsObject"
                Return clsRestriction.CharacterEnum.BeInSameLocationAsObject
            Case "BeLyingOnObject"
                Return clsRestriction.CharacterEnum.BeLyingOnObject
            Case "BeMemberOfGroup", "BeInGroup"
                Return clsRestriction.CharacterEnum.BeInGroup
            Case "BeOfGender"
                Return clsRestriction.CharacterEnum.BeOfGender
            Case "BeSittingOnObject"
                Return clsRestriction.CharacterEnum.BeSittingOnObject
            Case "BeStandingOnObject"
                Return clsRestriction.CharacterEnum.BeStandingOnObject
            Case "BeWearingObject"
                Return clsRestriction.CharacterEnum.BeWearingObject
            Case "BeWithinLocationGroup"
                Return clsRestriction.CharacterEnum.BeWithinLocationGroup
            Case "HaveProperty"
                Return clsRestriction.CharacterEnum.HaveProperty
            Case "BeInPosition"
                Return clsRestriction.CharacterEnum.BeInPosition
            Case "BeInsideObject"
                Return clsRestriction.CharacterEnum.BeInsideObject
            Case "BeOnObject"
                Return clsRestriction.CharacterEnum.BeOnObject
            Case "BeOnCharacter"
                Return clsRestriction.CharacterEnum.BeOnCharacter
            Case "BeVisibleToCharacter"
                Return clsRestriction.CharacterEnum.BeVisibleToCharacter
            Case Else
                Return Nothing
        End Select

    End Function

    Public Function EnumParseObject(ByVal sValue As String) As clsRestriction.ObjectEnum
        Select Case sValue
            Case "BeExactText"
                Return clsRestriction.ObjectEnum.BeExactText
            Case "BeAtLocation"
                Return clsRestriction.ObjectEnum.BeAtLocation
            Case "BeHeldByCharacter"
                Return clsRestriction.ObjectEnum.BeHeldByCharacter
            Case "BeHidden"
                Return clsRestriction.ObjectEnum.BeHidden
            Case "BeInGroup"
                Return clsRestriction.ObjectEnum.BeInGroup
            Case "BeInsideObject"
                Return clsRestriction.ObjectEnum.BeInsideObject
            Case "BeInState"
                Return clsRestriction.ObjectEnum.BeInState
            Case "BeOnObject"
                Return clsRestriction.ObjectEnum.BeOnObject
            Case "BePartOfCharacter"
                Return clsRestriction.ObjectEnum.BePartOfCharacter
            Case "BePartOfObject"
                Return clsRestriction.ObjectEnum.BePartOfObject
            Case "BeVisibleToCharacter"
                Return clsRestriction.ObjectEnum.BeVisibleToCharacter
            Case "BeWornByCharacter"
                Return clsRestriction.ObjectEnum.BeWornByCharacter
            Case "Exist"
                Return clsRestriction.ObjectEnum.Exist
            Case "HaveBeenSeenByCharacter"
                Return clsRestriction.ObjectEnum.HaveBeenSeenByCharacter
            Case "HaveProperty"
                Return clsRestriction.ObjectEnum.HaveProperty
            Case "BeObject"
                Return clsRestriction.ObjectEnum.BeObject
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseSubEventMeasure(ByVal sValue As String) As clsEvent.SubEvent.MeasureEnum
        Select Case sValue
            Case "Turns"
                Return clsEvent.SubEvent.MeasureEnum.Turns
            Case "Seconds"
                Return clsEvent.SubEvent.MeasureEnum.Seconds
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseSubEventWhat(ByVal sValue As String) As clsEvent.SubEvent.WhatEnum
        Select Case sValue
            Case "DisplayMessage"
                Return clsEvent.SubEvent.WhatEnum.DisplayMessage
            Case "ExecuteTask"
                Return clsEvent.SubEvent.WhatEnum.ExecuteTask
            Case "SetLook"
                Return clsEvent.SubEvent.WhatEnum.SetLook
            Case "UnsetTask"
                Return clsEvent.SubEvent.WhatEnum.UnsetTask
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseTaskType(ByVal sValue As String) As clsTask.TaskTypeEnum
        Select Case sValue
            Case "General"
                Return clsTask.TaskTypeEnum.General
            Case "Specific"
                Return clsTask.TaskTypeEnum.Specific
            Case "System"
                Return clsTask.TaskTypeEnum.System
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseSpecificType(ByVal sValue As String) As ReferencesType
        Select Case sValue
            Case "Character"
                Return ReferencesType.Character
            Case "Direction"
                Return ReferencesType.Direction
            Case "Number"
                Return ReferencesType.Number
            Case "Object"
                Return ReferencesType.Object
            Case "Text"
                Return ReferencesType.Text
            Case "Location"
                Return ReferencesType.Location
            Case "Item"
                Return ReferencesType.Item
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseCharacterType(ByVal sValue As String) As clsCharacter.CharacterTypeEnum
        Select Case sValue
            Case "NonPlayer"
                Return clsCharacter.CharacterTypeEnum.NonPlayer
            Case "Player"
                Return clsCharacter.CharacterTypeEnum.Player
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseVariableType(ByVal sValue As String) As clsVariable.VariableTypeEnum
        Select Case sValue
            Case "Numeric"
                Return clsVariable.VariableTypeEnum.Numeric
            Case "Text"
                Return clsVariable.VariableTypeEnum.Text
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function EnumParseGroupType(ByVal sValue As String) As clsGroup.GroupTypeEnum
        Select Case sValue
            Case "Characters"
                Return clsGroup.GroupTypeEnum.Characters
            Case "Locations"
                Return clsGroup.GroupTypeEnum.Locations
            Case "Objects"
                Return clsGroup.GroupTypeEnum.Objects
            Case Else
                Throw New Exception("Value " & sValue & " not parsed!")
                Return Nothing
        End Select
    End Function

    Public Function WriteEnum(ByVal e As clsAction.EndGameEnum) As String
        Select Case e
            Case clsAction.EndGameEnum.Win
                Return "Win"
            Case clsAction.EndGameEnum.Lose
                Return "Lose"
            Case clsAction.EndGameEnum.Neutral
                Return "Neutral"
            Case Else
                Return Nothing
        End Select
    End Function

    Public Function WriteEnum(ByVal w As clsEvent.SubEvent.WhatEnum) As String
        Select Case w
            Case clsEvent.SubEvent.WhatEnum.DisplayMessage
                Return "DisplayMessage"
            Case clsEvent.SubEvent.WhatEnum.ExecuteTask
                Return "ExecuteTask"
            Case clsEvent.SubEvent.WhatEnum.SetLook
                Return "SetLook"
            Case clsEvent.SubEvent.WhatEnum.UnsetTask
                Return "UnsetTask"
            Case Else
                Return Nothing
        End Select
    End Function

    Public Function WriteEnum(ByVal m As clsEvent.SubEvent.MeasureEnum) As String
        Select Case m
            Case clsEvent.SubEvent.MeasureEnum.Turns
                Return "Turns"
            Case clsEvent.SubEvent.MeasureEnum.Seconds
                Return "Seconds"
            Case Else
                Return Nothing
        End Select
    End Function

    Public Function WriteEnum(ByVal d As DirectionsEnum) As String
        Select Case d
            Case DirectionsEnum.Down
                Return "Down"
            Case DirectionsEnum.East
                Return "East"
            Case DirectionsEnum.In
                Return "In"
            Case DirectionsEnum.North
                Return "North"
            Case DirectionsEnum.NorthEast
                Return "NorthEast"
            Case DirectionsEnum.NorthWest
                Return "NorthWest"
            Case DirectionsEnum.Out
                Return "Out"
            Case DirectionsEnum.South
                Return "South"
            Case DirectionsEnum.SouthEast
                Return "SouthEast"
            Case DirectionsEnum.SouthWest
                Return "SouthWest"
            Case DirectionsEnum.Up
                Return "Up"
            Case DirectionsEnum.West
                Return "West"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal p As clsProperty.PropertyOfEnum) As String
        Select Case p
            Case clsProperty.PropertyOfEnum.Objects
                Return "Objects"
            Case clsProperty.PropertyOfEnum.Characters
                Return "Characters"
            Case clsProperty.PropertyOfEnum.Locations
                Return "Locations"
            Case clsProperty.PropertyOfEnum.AnyItem
                Return "AnyItem"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal t As clsProperty.PropertyTypeEnum) As String
        Select Case t
            Case clsProperty.PropertyTypeEnum.CharacterKey
                Return "CharacterKey"
            Case clsProperty.PropertyTypeEnum.Integer
                Return "Integer"
            Case clsProperty.PropertyTypeEnum.LocationGroupKey
                Return "LocationGroupKey"
            Case clsProperty.PropertyTypeEnum.LocationKey
                Return "LocationKey"
            Case clsProperty.PropertyTypeEnum.ObjectKey
                Return "ObjectKey"
            Case clsProperty.PropertyTypeEnum.SelectionOnly
                Return "SelectionOnly"
            Case clsProperty.PropertyTypeEnum.StateList
                Return "StateList"
            Case clsProperty.PropertyTypeEnum.ValueList
                Return "ValueList"
            Case clsProperty.PropertyTypeEnum.Text
                Return "Text"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal m As clsRestriction.MustEnum) As String
        Select Case m
            Case clsRestriction.MustEnum.Must
                Return "Must"
            Case clsRestriction.MustEnum.MustNot
                Return "MustNot"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal l As clsRestriction.LocationEnum) As String
        Select Case l
            Case clsRestriction.LocationEnum.BeInGroup
                Return "BeInGroup"
            Case clsRestriction.LocationEnum.HaveBeenSeenByCharacter
                Return "HaveBeenSeenByCharacter"
            Case clsRestriction.LocationEnum.HaveProperty
                Return "HaveProperty"
            Case clsRestriction.LocationEnum.BeLocation
                Return "BeLocation"
            Case clsRestriction.LocationEnum.Exist
                Return "Exist"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal o As clsRestriction.ObjectEnum) As String
        Select Case o
            Case clsRestriction.ObjectEnum.BeAtLocation
                Return "BeAtLocation"
            Case clsRestriction.ObjectEnum.BeHeldByCharacter
                Return "BeHeldByCharacter"
            Case clsRestriction.ObjectEnum.BeInGroup
                Return "BeInGroup"
            Case clsRestriction.ObjectEnum.BeInsideObject
                Return "BeInsideObject"
            Case clsRestriction.ObjectEnum.BeInState
                Return "BeInState"
            Case clsRestriction.ObjectEnum.BeOnObject
                Return "BeOnObject"
            Case clsRestriction.ObjectEnum.BePartOfCharacter
                Return "BePartOfCharacter"
            Case clsRestriction.ObjectEnum.BePartOfObject
                Return "BePartOfObject"
            Case clsRestriction.ObjectEnum.BeVisibleToCharacter
                Return "BeVisibleToCharacter"
            Case clsRestriction.ObjectEnum.BeWornByCharacter
                Return "BeWornByCharacter"
            Case clsRestriction.ObjectEnum.Exist
                Return "Exist"
            Case clsRestriction.ObjectEnum.HaveBeenSeenByCharacter
                Return "HaveBeenSeenByCharacter"
            Case clsRestriction.ObjectEnum.HaveProperty
                Return "HaveProperty"
            Case clsRestriction.ObjectEnum.BeExactText
                Return "BeExactText"
            Case clsRestriction.ObjectEnum.BeHidden
                Return "BeHidden"
            Case clsRestriction.ObjectEnum.BeObject
                Return "BeObject"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal c As clsRestriction.ItemEnum) As String
        Select Case c
            Case clsRestriction.ItemEnum.BeAtLocation
                Return "BeAtLocation"
            Case clsRestriction.ItemEnum.BeCharacter
                Return "BeCharacter"
            Case clsRestriction.ItemEnum.BeInSameLocationAsCharacter
                Return "BeInSameLocationAsCharacter"
            Case clsRestriction.ItemEnum.BeInSameLocationAsObject
                Return "BeInSameLocationAsObject"
            Case clsRestriction.ItemEnum.BeInsideObject
                Return "BeInsideObject"
            Case clsRestriction.ItemEnum.BeLocation
                Return "BeLocation"
            Case clsRestriction.ItemEnum.BeInGroup
                Return "BeInGroup"
            Case clsRestriction.ItemEnum.BeObject
                Return "BeObject"
            Case clsRestriction.ItemEnum.BeOnCharacter
                Return "BeOnCharacter"
            Case clsRestriction.ItemEnum.Exist
                Return "Exist"
            Case clsRestriction.ItemEnum.HaveProperty
                Return "HaveProperty"
            Case clsRestriction.ItemEnum.BeType
                Return "BeType"
            Case Else
                TODO()
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal c As clsRestriction.CharacterEnum) As String
        Select Case c
            Case clsRestriction.CharacterEnum.BeAlone
                Return "BeAlone"
            Case clsRestriction.CharacterEnum.BeAloneWith
                Return "BeAloneWith"
            Case clsRestriction.CharacterEnum.BeAtLocation
                Return "BeAtLocation"
            Case clsRestriction.CharacterEnum.BeCharacter
                Return "BeCharacter"
            Case clsRestriction.CharacterEnum.BeInConversationWith
                Return "BeInConversationWith"
            Case clsRestriction.CharacterEnum.Exist
                Return "Exist"
            Case clsRestriction.CharacterEnum.HaveRouteInDirection
                Return "HaveRouteInDirection"
            Case clsRestriction.CharacterEnum.HaveSeenCharacter
                Return "HaveSeenCharacter"
            Case clsRestriction.CharacterEnum.HaveSeenLocation
                Return "HaveSeenLocation"
            Case clsRestriction.CharacterEnum.HaveSeenObject
                Return "HaveSeenObject"
            Case clsRestriction.CharacterEnum.BeHoldingObject
                Return "BeHoldingObject"
            Case clsRestriction.CharacterEnum.BeInSameLocationAsCharacter
                Return "BeInSameLocationAsCharacter"
            Case clsRestriction.CharacterEnum.BeInSameLocationAsObject
                Return "BeInSameLocationAsObject"
            Case clsRestriction.CharacterEnum.BeLyingOnObject
                Return "BeLyingOnObject"
            Case clsRestriction.CharacterEnum.BeInGroup
                Return "BeInGroup"
            Case clsRestriction.CharacterEnum.BeOfGender
                Return "BeOfGender"
            Case clsRestriction.CharacterEnum.BeSittingOnObject
                Return "BeSittingOnObject"
            Case clsRestriction.CharacterEnum.BeStandingOnObject
                Return "BeStandingOnObject"
            Case clsRestriction.CharacterEnum.BeWearingObject
                Return "BeWearingObject"
            Case clsRestriction.CharacterEnum.BeWithinLocationGroup
                Return "BeWithinLocationGroup"
            Case clsRestriction.CharacterEnum.HaveProperty
                Return "HaveProperty"
            Case clsRestriction.CharacterEnum.BeInPosition
                Return "BeInPosition"
            Case clsRestriction.CharacterEnum.BeInsideObject
                Return "BeInsideObject"
            Case clsRestriction.CharacterEnum.BeOnObject
                Return "BeOnObject"
            Case clsRestriction.CharacterEnum.BeOnCharacter
                Return "BeOnCharacter"
            Case clsRestriction.CharacterEnum.BeVisibleToCharacter
                Return "BeVisibleToCharacter"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal v As clsRestriction.VariableEnum) As String
        Select Case v
            Case clsRestriction.VariableEnum.EqualTo
                Return "EqualTo"
            Case clsRestriction.VariableEnum.GreaterThan
                Return "GreaterThan"
            Case clsRestriction.VariableEnum.GreaterThanOrEqualTo
                Return "GreaterThanOrEqualTo"
            Case clsRestriction.VariableEnum.LessThan
                Return "LessThan"
            Case clsRestriction.VariableEnum.LessThanOrEqualTo
                Return "LessThanOrEqualTo"
            Case clsRestriction.VariableEnum.Contain
                Return "Contain"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal ba As clsTask.BeforeAfterEnum) As String
        Select Case ba
            Case clsTask.BeforeAfterEnum.After
                Return "After"
            Case clsTask.BeforeAfterEnum.Before
                Return "Before"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal mc As clsAction.MoveCharacterToEnum) As String
        Select Case mc
            Case clsAction.MoveCharacterToEnum.InDirection
                Return "InDirection"
            Case clsAction.MoveCharacterToEnum.ToLocation
                Return "ToLocation"
            Case clsAction.MoveCharacterToEnum.ToLocationGroup
                Return "ToLocationGroup"
            Case clsAction.MoveCharacterToEnum.ToLyingOn
                Return "ToLyingOn"
            Case clsAction.MoveCharacterToEnum.ToSameLocationAs
                Return "ToSameLocationAs"
            Case clsAction.MoveCharacterToEnum.ToSittingOn
                Return "ToSittingOn"
            Case clsAction.MoveCharacterToEnum.ToStandingOn
                Return "ToStandingOn"
            Case clsAction.MoveCharacterToEnum.ToSwitchWith
                Return "ToSwitchWith"
            Case clsAction.MoveCharacterToEnum.InsideObject
                Return "InsideObject"
            Case clsAction.MoveCharacterToEnum.OntoCharacter
                Return "OntoCharacter"
            Case clsAction.MoveCharacterToEnum.ToParentLocation
                Return "ToParentLocation"
            Case clsAction.MoveCharacterToEnum.ToGroup
                Return "ToGroup"
            Case clsAction.MoveCharacterToEnum.FromGroup
                Return "FromGroup"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal st As clsAction.SetTasksEnum) As String
        Select Case st
            Case clsAction.SetTasksEnum.Execute
                Return "Execute"
            Case clsAction.SetTasksEnum.Unset
                Return "Unset"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal ml As clsAction.MoveLocationToEnum) As String
        Select Case ml
            Case clsAction.MoveLocationToEnum.FromGroup
                Return "FromGroup"
            Case clsAction.MoveLocationToEnum.ToGroup
                Return "ToGroup"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal mo As clsAction.MoveObjectToEnum) As String
        Select Case mo
            Case clsAction.MoveObjectToEnum.InsideObject
                Return "InsideObject"
            Case clsAction.MoveObjectToEnum.OntoObject
                Return "OntoObject"
            Case clsAction.MoveObjectToEnum.ToCarriedBy
                Return "ToCarriedBy"
            Case clsAction.MoveObjectToEnum.ToLocation
                Return "ToLocation"
            Case clsAction.MoveObjectToEnum.ToLocationGroup
                Return "ToLocationGroup"
            Case clsAction.MoveObjectToEnum.ToPartOfCharacter
                Return "ToPartOfCharacter"
            Case clsAction.MoveObjectToEnum.ToPartOfObject
                Return "ToPartOfObject"
            Case clsAction.MoveObjectToEnum.ToSameLocationAs
                Return "ToSameLocationAs"
            Case clsAction.MoveObjectToEnum.ToWornBy
                Return "ToWornBy"
            Case clsAction.MoveObjectToEnum.ToGroup
                Return "ToGroup"
            Case clsAction.MoveObjectToEnum.FromGroup
                Return "FromGroup"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal tt As clsTask.TaskTypeEnum) As String
        Select Case tt
            Case clsTask.TaskTypeEnum.General
                Return "General"
            Case clsTask.TaskTypeEnum.Specific
                Return "Specific"
            Case clsTask.TaskTypeEnum.System
                Return "System"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal s As ReferencesType) As String
        Select Case s
            Case ReferencesType.Character
                Return "Character"
            Case ReferencesType.Direction
                Return "Direction"
            Case ReferencesType.Number
                Return "Number"
            Case ReferencesType.Object
                Return "Object"
            Case ReferencesType.Text
                Return "Text"
            Case ReferencesType.Location
                Return "Location"
            Case ReferencesType.Item
                Return "Item"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal ct As clsCharacter.CharacterTypeEnum) As String
        Select Case ct
            Case clsCharacter.CharacterTypeEnum.NonPlayer
                Return "NonPlayer"
            Case clsCharacter.CharacterTypeEnum.Player
                Return "Player"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal vt As clsVariable.VariableTypeEnum) As String
        Select Case vt
            Case clsVariable.VariableTypeEnum.Numeric
                Return "Numeric"
            Case clsVariable.VariableTypeEnum.Text
                Return "Text"
        End Select
        Return Nothing
    End Function

    Public Function WriteEnum(ByVal gt As clsGroup.GroupTypeEnum) As String
        Select Case gt
            Case clsGroup.GroupTypeEnum.Characters
                Return "Characters"
            Case clsGroup.GroupTypeEnum.Locations
                Return "Locations"
            Case clsGroup.GroupTypeEnum.Objects
                Return "Objects"
        End Select
        Return Nothing
    End Function

    ' cos Val() is not international-friendly...
    Public Function SafeDec(ByVal Expression As Object) As Decimal
        Try
            If Expression Is Nothing OrElse IsDBNull(Expression) Then Return 0
            If IsNumeric(Expression) Then Return CDec(Expression) Else Return 0
        Catch ex As Exception
            ErrMsg("SafeDec error with expression <" & Expression.ToString & ">", ex)
            Return 0
        End Try
    End Function

    Public Function SafeDbl(ByVal Expression As Object) As Double
        Try
            Dim sExpression As String = SafeString(Expression)
            If sExpression.Contains(",") Then sExpression = sExpression.Replace(",", System.Globalization.CultureInfo.CurrentCulture.NumberFormat.NumberGroupSeparator)
            If sExpression.Contains(".") Then sExpression = sExpression.Replace(".", System.Globalization.CultureInfo.CurrentCulture.NumberFormat.NumberDecimalSeparator)

            If sExpression = "" OrElse Not IsNumeric(sExpression) Then Return 0
            Dim df As Double = CDbl(sExpression)
            If Double.IsInfinity(df) OrElse Double.IsNaN(df) Then Return 0 Else Return df
        Catch ex As Exception
            ErrMsg("SafeDbl error with expression <" & Expression.ToString & ">", ex)
            Return 0
        End Try
    End Function

    Public Function SafeInt(ByVal Expression As Object) As Integer
        Try
            If Expression Is Nothing OrElse IsDBNull(Expression) Then Return 0
            If IsNumeric(Expression) Then Return CInt(Int(Expression)) Else Return 0
        Catch exOF As OverflowException
            Return Integer.MaxValue
        Catch ex As Exception
            ErrMsg("SafeInt error with expression <" & Expression.ToString & ">", ex)
            Return 0
        End Try
    End Function

    Public Function SafeBool(ByVal Expression As Object) As Boolean
        Try
            If Expression Is Nothing OrElse IsDBNull(Expression) Then Return False
            Select Case Expression.ToString.ToUpper
                Case True.ToString.ToUpper
                    Return True
                Case False.ToString.ToUpper
                    Return False
                Case Else
                    Try
                        Return CBool(Expression)
                    Catch ex As Exception
                        Return False
                    End Try
            End Select
        Catch ex As Exception
            ErrMsg("SafeBool error with expression <" & Expression.ToString & ">", ex)
            Return False
        End Try
    End Function

    Public Function SafeString(ByVal Expression As Object) As String
        Try
            If Expression Is Nothing OrElse IsDBNull(Expression) Then Return ""
            Return Expression.ToString
        Catch ex As Exception
            ErrMsg("SafeString error with expression <" & Expression.ToString & ">", ex)
            Return ""
        End Try
    End Function

    Public Function SafeDate(ByVal Expression As Object) As Date
        Try
            If Expression Is Nothing OrElse IsDBNull(Expression) OrElse Not IsDate(Expression) Then Return New Date
            Return CDate(Expression)
        Catch ex As Exception
            ErrMsg("SafeDate error with expression <" & Expression.ToString & ">", ex)
            Return New Date
        End Try
    End Function

    Public Class EventOrWalkControl
        Implements ICloneable

        Public Enum ControlEnum
            Start
            [Stop]
            Suspend
            [Resume]
        End Enum
        Public eControl As ControlEnum

        Public Enum CompleteOrNotEnum
            Completion
            UnCompletion
        End Enum
        Public eCompleteOrNot As CompleteOrNotEnum

        Public sTaskKey As String

        Private Function Clone() As Object Implements System.ICloneable.Clone
            Return Me.MemberwiseClone
        End Function
        Public Function CloneMe() As EventOrWalkControl
            Return CType(Me.Clone, EventOrWalkControl)
        End Function

    End Class

    Public Class FromTo
        Implements ICloneable

        Public iFrom As Integer
        Public iTo As Integer
        Private iValue As Integer = Integer.MinValue

        Public ReadOnly Property Value() As Integer
            Get
                If iValue = Integer.MinValue Then
                    iValue = Random(iFrom, iTo) ' CInt(Rnd() * (iTo - iFrom)) + iFrom
                End If
                Return iValue
            End Get
        End Property

        Public Sub Reset()
            iValue = Integer.MinValue
        End Sub

        Private Function Clone() As Object Implements System.ICloneable.Clone
            Return Me.MemberwiseClone
        End Function
        Public Function CloneMe() As FromTo
            Return CType(Clone(), FromTo)
        End Function

        Public Shadows ReadOnly Property ToString() As String
            Get
                If iFrom = iTo Then
                    Return iFrom.ToString
                Else
                    Return iFrom.ToString & " to " & iTo.ToString
                End If
            End Get
        End Property

    End Class

    <DebuggerDisplay("{Description}")>
    Public Class SingleDescription
        Implements ICloneable

        Public Enum DisplayWhenEnum
            StartDescriptionWithThis
            StartAfterDefaultDescription
            AppendToPreviousDescription
        End Enum

        Friend Restrictions As New RestrictionArrayList
        Friend eDisplayWhen As DisplayWhenEnum = DisplayWhenEnum.AppendToPreviousDescription
        Friend sTabLabel As String = ""
        Public Description As String = ""
        Friend DisplayOnce As Boolean = False
        Friend Property ReturnToDefault As Boolean = False
        Friend Displayed As Boolean

        Private Function Clone() As Object Implements System.ICloneable.Clone
            Return Me.MemberwiseClone
        End Function
        Public Function CloneMe() As SingleDescription
            Return CType(Clone(), SingleDescription)
        End Function

    End Class

    Public Class Description
        Inherits List(Of SingleDescription)

        Public Sub New()
            Me.New("")
        End Sub
        Public Sub New(ByVal sDescription As String)

            Dim sd As New SingleDescription
            With sd
                sd.Description = sDescription
                sd.eDisplayWhen = SingleDescription.DisplayWhenEnum.StartDescriptionWithThis
            End With
            MyBase.Add(sd)

        End Sub

        Public Function ReferencesKey(ByVal sKey As String) As Integer

            Dim iCount As Integer = 0
            For Each m As SingleDescription In Me
                For Each r As clsRestriction In m.Restrictions
                    If r.ReferencesKey(sKey) Then iCount += 1
                Next
            Next
            Return iCount

        End Function

        Public Function DeleteKey(ByVal sKey As String) As Boolean

            For Each m As SingleDescription In Me
                If Not m.Restrictions.DeleteKey(sKey) Then Return False
            Next

            Return True

        End Function


        'Should we add spaces between one desctiption tab and another
        Private Function AddSpace(ByVal sText As String) As Boolean
            If sText = "" Then Return False
            If sText.EndsWith(" ") OrElse sText.EndsWith(vbLf) Then Return False

            ' Add spaces after end of sentences
            If sText.EndsWith(".") OrElse sText.EndsWith("!") OrElse sText.EndsWith("?") Then Return True

            ' If it's a function then add a space -small chance the function could evaluate to "", but we'll take that chance
            If sText.EndsWith("%") Then Return True

            ' If text ends in an OO function, return True
            If New System.Text.RegularExpressions.Regex(".*?(%?[A-Za-z][\w\|_-]*%?)(\.%?[A-Za-z][\w\|_-]*%?(\([A-Za-z ,_]+?\))?)+").IsMatch(sText) Then Return True

            ' Otherwise return False
            Return False
        End Function

        ' If bTesting is set, we're just testing the value, so don't mark text as having been output
        Public Shadows ReadOnly Property ToString(Optional ByVal bTesting As Boolean = False) As String
            Get
                Dim sb As New System.Text.StringBuilder

                Dim sRestrictionTextIn As String = UserSession.sRestrictionText
                Dim iRestNumIn As Integer = UserSession.iRestNum
                Dim sRouteErrorIn As String = UserSession.sRouteError

                Try
                    For Each sd As SingleDescription In Me
                        With sd
                            If Not sd.DisplayOnce OrElse Not sd.Displayed Then
                                Dim bDisplayed As Boolean = False
                                If sd.Restrictions.Count = 0 OrElse UserSession.PassRestrictions(sd.Restrictions) Then
                                    Select Case sd.eDisplayWhen
                                        Case SingleDescription.DisplayWhenEnum.AppendToPreviousDescription
                                            If AddSpace(sb.ToString) Then sb.Append("  ")
                                            sb.Append("<>" & sd.Description)
                                        Case SingleDescription.DisplayWhenEnum.StartAfterDefaultDescription
                                            If sd Is Me(0) Then
                                                sb = New System.Text.StringBuilder(sd.Description)
                                            Else
                                                Dim sDefault As String = Me(0).Description
                                                If AddSpace(sDefault) Then sDefault &= "  "
                                                sb = New System.Text.StringBuilder(sDefault & sd.Description)
                                            End If
                                        Case SingleDescription.DisplayWhenEnum.StartDescriptionWithThis
                                            sb = New System.Text.StringBuilder(sd.Description)
                                    End Select
                                    bDisplayed = True
                                Else
                                    If UserSession.sRestrictionText <> "" Then
                                        Select Case sd.eDisplayWhen
                                            Case SingleDescription.DisplayWhenEnum.AppendToPreviousDescription
                                                If AddSpace(sb.ToString) Then sb.Append("  ")
                                                sb.Append("<>" & UserSession.sRestrictionText)
                                            Case SingleDescription.DisplayWhenEnum.StartAfterDefaultDescription
                                                If sd Is Me(0) Then
                                                    sb = New System.Text.StringBuilder(UserSession.sRestrictionText)
                                                Else
                                                    Dim sDefault As String = Me(0).Description
                                                    If AddSpace(sDefault) Then sDefault &= "  "
                                                    sb = New System.Text.StringBuilder(sDefault & UserSession.sRestrictionText)
                                                End If
                                            Case SingleDescription.DisplayWhenEnum.StartDescriptionWithThis
                                                sb = New System.Text.StringBuilder(UserSession.sRestrictionText)
                                        End Select
                                    End If
                                End If
                                If .DisplayOnce Then
                                    ' Is this right, or should it mark Displayed = True if any text is output?
                                    If Not (bTesting OrElse UserSession.bTestingOutput) AndAlso bDisplayed Then
                                        sd.Displayed = True
                                        If sd.ReturnToDefault Then
                                            For Each sd2 As SingleDescription In Me
                                                sd2.Displayed = False
                                                If sd2 Is sd Then Exit For
                                            Next
                                        End If
                                    End If
                                    Return sb.ToString
                                End If
                            End If
                        End With
                    Next

                Catch
                Finally
                    UserSession.sRestrictionText = sRestrictionTextIn
                    UserSession.iRestNum = iRestNumIn
                    UserSession.sRouteError = sRouteErrorIn
                End Try

                Return sb.ToString
            End Get
        End Property

        Public Function Copy() As Description
            Dim d As New Description
            d.Clear()
            For Each sd As SingleDescription In Me
                Dim sdNew As New SingleDescription
                sdNew.Description = sd.Description
                sdNew.eDisplayWhen = sd.eDisplayWhen
                sdNew.Restrictions = sd.Restrictions.Copy
                sdNew.DisplayOnce = sd.DisplayOnce
                sdNew.ReturnToDefault = sd.ReturnToDefault
                sdNew.sTabLabel = sd.sTabLabel
                d.Add(sdNew)
            Next
            Return d
        End Function

    End Class


    Public Sub TODO(Optional ByVal sFunction As String = "")
        If sFunction = "" Then sFunction = "This section" Else sFunction = "Function """ & sFunction & """"
        Glue.ShowInfo("TODO: " & sFunction & " still has to be completed. Please inform Campbell of what you were doing.")
    End Sub

    Public Class clsSearchOptions

        Public Enum SearchInWhatEnum As Integer
            Uninitialised = -1
            AllItems = 0
            NonLibraryItems = 1
        End Enum

        Public sLastKey As String = ""
        Public sLastSearch As String = ""
        Private eSearchInWhat As SearchInWhatEnum = SearchInWhatEnum.Uninitialised
        Public Property SearchInWhat() As SearchInWhatEnum
            Get
                If eSearchInWhat = SearchInWhatEnum.Uninitialised Then
                    eSearchInWhat = CType(GetSetting("ADRIFT", "Generator", "SearchInWhat", CInt(SearchInWhatEnum.NonLibraryItems).ToString), SearchInWhatEnum)
                End If
                Return eSearchInWhat
            End Get
            Set(ByVal value As SearchInWhatEnum)
                eSearchInWhat = value
                SaveSetting("ADRIFT", "Generator", "SearchInWhat", CInt(value).ToString)
            End Set
        End Property

        Public bSearchMatchCase As Boolean = False
        Public bFindExactWord As Boolean = False

    End Class

    Public SearchOptions As New clsSearchOptions

    Public Sub SearchAndReplace(ByVal sFind As String, ByVal sReplace As String, Optional ByVal bSilent As Boolean = False)
        Dim iReplacements As Integer = 0

        With SearchOptions
            For Each item As clsItem In Adventure.dictAllItems.Values
                Dim bLookIn As Boolean = False
                Select Case .SearchInWhat
                    Case clsSearchOptions.SearchInWhatEnum.AllItems
                        bLookIn = True
                    Case clsSearchOptions.SearchInWhatEnum.NonLibraryItems
                        bLookIn = Not item.IsLibrary
                End Select
                If bLookIn Then iReplacements += item.SearchAndReplace(sFind, sReplace)
            Next
        End With
    End Sub

End Module
