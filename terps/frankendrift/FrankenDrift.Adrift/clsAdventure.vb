
Imports System.Drawing

Public Class clsAdventure

    Friend dictAllItems As ItemDictionary
    Public htblLocations As LocationHashTable
    Friend htblObjects As ObjectHashTable
    Friend htblTasks As TaskHashTable
    Friend htblEvents As EventHashTable
    Friend htblCharacters As CharacterHashTable
    Friend htblGroups As GroupHashTable
    Friend htblVariables As VariableHashTable
    Friend htblALRs As ALRHashTable
    Friend htblHints As HintHashTable
    Friend htblUDFs As UDFHashTable
    Friend htblAllProperties As PropertyHashTable
    Friend htblObjectProperties As PropertyHashTable
    Friend htblCharacterProperties As PropertyHashTable
    Friend htblLocationProperties As PropertyHashTable
    Friend htblAllTopicKeys As StringArrayList
    Friend htblSynonyms As SynonymHashTable
    Friend listKnownWords As List(Of String)
    Public Map As clsMap

    Private oIntroduction As Description
    Private oWinningText As Description
    Private sTitle As String
    Private sAuthor As String
    Private sNotUnderstood As String
    Private sFilename As String
    Private sFullPath As String
    Private bChanged As Boolean

    Public ReadOnly Property DynamicObjects As IEnumerable(Of clsObject)
        Get
            Return htblObjects.Values.Where(Function(o) o.IsStatic = False)
        End Get
    End Property

    Public listExcludedItems As New HashSet(Of String) ' Library items removed, so we don't keep reloading them


    Public dVersion As Double
    Private iMaxScore As Integer
    Public iScore As Integer
    Public Turns As Integer
    Public sReferencedText(4) As String
    Friend sConversationCharKey As String ' Who we are in conversation with
    Friend sConversationNode As String ' Where we currently are in the conversation tree
    Friend sGameFilename As String
    Public eGameState As clsAction.EndGameEnum = clsAction.EndGameEnum.Running
    Friend bDisplayedWinLose As Boolean = False
    Friend qTasksToRun As New Generic.Queue(Of String)
    Friend iElapsed As Integer
    Friend sDirectionsRE(11) As String
    Friend lstTasks As List(Of clsTask) ' Simply a list, sorted by priority
    Friend dictRandValues As New Dictionary(Of String, List(Of Integer))


    Public Enum TaskExecutionEnum
        HighestPriorityTask
        HighestPriorityPassingTask ' v4 logic - tries to execute first matching passing task, if that fails first matching failing task
    End Enum
    Friend TaskExecution As TaskExecutionEnum = TaskExecutionEnum.HighestPriorityTask

    Private _UserStatus As String
    Public Property sUserStatus As String
        Get
            Return _UserStatus
        End Get
        Set(value As String)
            _UserStatus = value
        End Set
    End Property

    Private sCoverFilename As String
    Friend Property CoverFilename As String
        Get
            Return sCoverFilename
        End Get
        Set(value As String)
            If value <> sCoverFilename Then
                Try
                    If BabelTreatyInfo.Stories(0).Cover Is Nothing Then BabelTreatyInfo.Stories(0).Cover = New clsBabelTreatyInfo.clsStory.clsCover
                    With BabelTreatyInfo.Stories(0).Cover
                        .imgCoverArt = Nothing
                        If IO.File.Exists(value) Then
                            Dim iLength As Integer = CInt(FileLen(value))
                            Dim bytImage As Byte()
                            Dim fs As New IO.FileStream(value, IO.FileMode.Open, IO.FileAccess.Read)
                            ReDim bytImage(iLength - 1)
                            fs.Read(bytImage, 0, CInt(iLength))
                            .imgCoverArt = bytImage
                            fs.Close()
                            .Format = IO.Path.GetExtension(value).ToLower.Substring(1)
                        End If
                    End With
                Catch ex As Exception
                    ErrMsg("Failed to set Cover Art", ex)
                End Try
                sCoverFilename = value
            End If
        End Set
    End Property

    Friend Password As String

    Friend lCharMentionedThisTurn As New Generic.Dictionary(Of clsCharacter.GenderEnum, Generic.List(Of String))

    Public BabelTreatyInfo As New clsBabelTreatyInfo


    Public Class v4Media
        Public iOffset As Integer
        Public iLength As Integer
        Public bImage As Boolean ' else Sound

        Public Sub New(ByVal iOffset As Integer, iLength As Integer, bImage As Boolean)
            Me.iOffset = iOffset
            Me.iLength = iLength
            Me.bImage = bImage
        End Sub
    End Class
    Public dictv4Media As Dictionary(Of String, v4Media) ' Filename


    Public ReadOnly Property Version As String
        Get
            If Adventure.dVersion = 0 Then Return "N/A"
            Dim sVersion As String = String.Format("{0:0.000000}", Adventure.dVersion) ' E.g. 5.000020
            Return sVersion(0) & "." & CInt(sVersion.Substring(2, 2)) & "." & CInt(sVersion.Substring(4, 4))
        End Get
    End Property


    Public Property Changed() As Boolean
        Get
            Return bChanged
        End Get
        Set(ByVal Value As Boolean)
            bChanged = Value
        End Set
    End Property

    Private dtLastUpdated As Date
    Friend Property LastUpdated() As Date
        Get
            If dtLastUpdated > Date.MinValue Then
                Return dtLastUpdated
            Else
                Return Now
            End If
        End Get
        Set(ByVal value As Date)
            dtLastUpdated = value
        End Set
    End Property

    Public Property KeyPrefix As String

    Private oPlayer As clsCharacter
    Public Property Player() As clsCharacter
        Get
            If oPlayer Is Nothing Then
                For Each ch As clsCharacter In Adventure.htblCharacters.Values
                    If ch.CharacterType = clsCharacter.CharacterTypeEnum.Player Then
                        oPlayer = ch
                        Return ch
                    End If
                Next
                If Adventure.htblCharacters.Count > 0 Then
                    For Each ch As clsCharacter In Adventure.htblCharacters.Values
                        oPlayer = ch
                        Return ch
                    Next
                End If
            Else
                Return oPlayer
            End If
            Return Nothing
        End Get
        Set(ByVal Value As clsCharacter)
            oPlayer = Value
        End Set
    End Property


    ' Returns a list of all references to images within the adventure
    Public ReadOnly Property Images As List(Of String)
        Get
            Dim lImages As New List(Of String)
            If Adventure.CoverFilename <> "" Then lImages.Add(Adventure.CoverFilename)

            For Each sd As SingleDescription In AllDescriptions ' d
                Dim s As String = sd.Description
                s = s.Replace(" src =", " src=")
                Dim i As Integer = s.IndexOf("<img ")
                While i > -1
                    Dim j As Integer = s.IndexOf(" src=", i)
                    If j > -1 Then
                        Dim k As Integer = s.IndexOf(">", i)
                        Dim sTag As String = s.Substring(j + 5, k - j - 5)
                        If sTag.StartsWith("""") Then sTag = sTag.Substring(1)
                        If sTag.EndsWith("""") Then sTag = sTag.Substring(0, sTag.Length - 1)
                        If Not lImages.Contains(sTag) AndAlso (IO.File.Exists(sTag) OrElse sTag.StartsWith("http")) Then lImages.Add(sTag)
                    End If
                    i = s.IndexOf("<img ", i + 1)
                End While
            Next

            Return lImages
        End Get
    End Property



    Friend ReadOnly Property AllDescriptions As List(Of SingleDescription)
        Get
            Dim lDescriptions As New List(Of SingleDescription)
            For Each sd As SingleDescription In Adventure.Introduction
                lDescriptions.Add(sd)
            Next
            For Each sd As SingleDescription In Adventure.WinningText
                lDescriptions.Add(sd)
            Next
            For Each itm As clsItem In Adventure.dictAllItems.Values
                For Each d As Description In itm.AllDescriptions
                    For Each sd As SingleDescription In d
                        lDescriptions.Add(sd)
                    Next
                Next
            Next
            Return lDescriptions
        End Get
    End Property


    Public BlorbMappings As New Dictionary(Of String, Integer)


    Public Property Score() As Integer
        Get
            If htblVariables.ContainsKey("Score") Then
                iScore = htblVariables("Score").IntValue
            End If
            Return iScore
        End Get
        Set(ByVal value As Integer)
            If value <> iScore Then
                If htblVariables.ContainsKey("Score") Then
                    htblVariables("Score").IntValue = value
                End If
            End If
            iScore = value
            UserSession.UpdateStatusBar()
        End Set
    End Property

    Public Property MaxScore() As Integer
        Get
            If htblVariables.ContainsKey("MaxScore") Then
                iMaxScore = htblVariables("MaxScore").IntValue
            End If
            Return iMaxScore
        End Get
        Set(ByVal value As Integer)
            If value <> iMaxScore Then
                If htblVariables.ContainsKey("MaxScore") Then
                    htblVariables("MaxScore").IntValue = value
                End If
            End If
            iMaxScore = value
        End Set
    End Property

#Region "Key Stuff"
    Public Function IsItemLibrary(ByVal sKey As String) As Boolean

        If AllKeys.Contains(sKey) Then
            Select Case GetTypeFromKeyNice(sKey)
                Case "Location"
                    Return htblLocations(sKey).IsLibrary
                Case "Object"
                    Return htblObjects(sKey).IsLibrary
                Case "Task"
                    Return htblTasks(sKey).IsLibrary
                Case "Event"
                    Return htblEvents(sKey).IsLibrary
                Case "Character"
                    Return htblCharacters(sKey).IsLibrary AndAlso sKey <> Player.Key
                Case "Group"
                    Return htblGroups(sKey).IsLibrary
                Case "Variable"
                    Return htblVariables(sKey).IsLibrary
                Case "Text Override"
                    Return htblALRs(sKey).IsLibrary
                Case "Hint"
                    Return htblHints(sKey).IsLibrary
                Case "Property"
                    Return htblAllProperties(sKey).IsLibrary
                Case "UserFunction"
                    Return htblUDFs(sKey).IsLibrary
                Case "Synonym"
                    Return htblSynonyms(sKey).IsLibrary
            End Select
        Else
            Return False
        End If

    End Function


    Public Function GetItemFromKey(ByVal sKey As String) As clsItem

        If sKey = "" Then Return Nothing

        If htblLocations.ContainsKey(sKey) Then Return htblLocations(sKey)
        If htblObjects.ContainsKey(sKey) Then Return htblObjects(sKey)
        If htblTasks.ContainsKey(sKey) Then Return htblTasks(sKey)
        If htblEvents.ContainsKey(sKey) Then Return htblEvents(sKey)
        If htblCharacters.ContainsKey(sKey) OrElse sKey = THEPLAYER Then Return htblCharacters(sKey)
        If htblGroups.ContainsKey(sKey) Then Return htblGroups(sKey)
        If htblVariables.ContainsKey(sKey) Then Return htblVariables(sKey)
        If htblALRs.ContainsKey(sKey) Then Return htblALRs(sKey)
        If htblHints.ContainsKey(sKey) Then Return htblHints(sKey)
        If htblAllProperties.ContainsKey(sKey) Then Return htblAllProperties(sKey)
        If htblUDFs.ContainsKey(sKey) Then Return htblUDFs(sKey)
        If htblSynonyms.ContainsKey(sKey) Then Return htblSynonyms(sKey)

        Return Nothing

    End Function


    Public Function GetTypeFromKey(ByVal sKey As String) As System.Type

        GetTypeFromKey = Nothing

        Select Case sKey
            Case "ReferencedObject", "ReferencedObjects", "ReferencedObject1", "ReferencedObject2", "ReferencedObject3", "ReferencedObject4", "ReferencedObject5"
                Return GetType(clsObject)
            Case "ReferencedCharacter", "ReferencedCharacters", "ReferencedCharacter1", "ReferencedCharacter2", "ReferencedCharacter3", "ReferencedCharacter4", "ReferencedCharacter5"
                Return GetType(clsCharacter)
            Case "ReferencedLocation"
                Return GetType(clsLocation)
        End Select
        If htblLocations.ContainsKey(sKey) Then Return htblLocations(sKey).GetType
        If htblObjects.ContainsKey(sKey) Then Return htblObjects(sKey).GetType
        If htblTasks.ContainsKey(sKey) Then Return htblTasks(sKey).GetType
        If htblEvents.ContainsKey(sKey) Then Return htblEvents(sKey).GetType
        If htblCharacters.ContainsKey(sKey) OrElse sKey = THEPLAYER Then Return htblCharacters(sKey).GetType
        If htblGroups.ContainsKey(sKey) Then Return htblGroups(sKey).GetType
        If htblVariables.ContainsKey(sKey) Then Return htblVariables(sKey).GetType
        If htblALRs.ContainsKey(sKey) Then Return htblALRs(sKey).GetType
        If htblHints.ContainsKey(sKey) Then Return htblHints(sKey).GetType
        If htblAllProperties.ContainsKey(sKey) Then Return htblAllProperties(sKey).GetType
        If htblUDFs.ContainsKey(sKey) Then Return htblUDFs(sKey).GetType
        If htblSynonyms.ContainsKey(sKey) Then Return htblSynonyms(sKey).GetType

    End Function


    Public Function GetTypeFromKeyNice(ByVal sKey As String, Optional ByVal bPlural As Boolean = False) As String

        If bPlural Then
            If htblLocations.ContainsKey(sKey) Then Return "Locations"
            If htblObjects.ContainsKey(sKey) Then Return "Objects"
            If htblTasks.ContainsKey(sKey) Then Return "Tasks"
            If htblEvents.ContainsKey(sKey) Then Return "Events"
            If htblCharacters.ContainsKey(sKey) Then Return "Characters"
            If htblGroups.ContainsKey(sKey) Then Return "Groups"
            If htblVariables.ContainsKey(sKey) Then Return "Variables"
            If htblALRs.ContainsKey(sKey) Then Return "Text Overrides"
            If htblHints.ContainsKey(sKey) Then Return "Hints"
            If htblAllProperties.ContainsKey(sKey) Then Return "Properties"
            If htblUDFs.ContainsKey(sKey) Then Return "User Functions"
            If htblSynonyms.ContainsKey(sKey) Then Return "Synonyms"
        Else
            If htblLocations.ContainsKey(sKey) Then Return "Location"
            If htblObjects.ContainsKey(sKey) Then Return "Object"
            If htblTasks.ContainsKey(sKey) Then Return "Task"
            If htblEvents.ContainsKey(sKey) Then Return "Event"
            If htblCharacters.ContainsKey(sKey) Then Return "Character"
            If htblGroups.ContainsKey(sKey) Then Return "Group"
            If htblVariables.ContainsKey(sKey) Then Return "Variable"
            If htblALRs.ContainsKey(sKey) Then Return "Text Override"
            If htblHints.ContainsKey(sKey) Then Return "Hint"
            If htblAllProperties.ContainsKey(sKey) Then Return "Property"
            If htblUDFs.ContainsKey(sKey) Then Return "User Function"
            If htblSynonyms.ContainsKey(sKey) Then Return "Synonym"
        End If

        Return ""

    End Function


    Public Function GetNameFromKey(ByVal sKey As String, Optional ByVal bQuoted As Boolean = True, Optional ByVal bPrefixItem As Boolean = True, Optional ByVal bPCase As Boolean = False) As String

        Dim sQ As String = ""
        Dim sO As String = ""
        Dim sC As String = ""

        GetNameFromKey = Nothing

        If sKey Is Nothing Then
            ErrMsg("Bad Key")
            Return Nothing
        End If

        If bQuoted Then
            sQ = "'"
        Else
            sO = "[ "
            sC = " ]"
        End If


        If sKey.StartsWith("Referenced") Then
            Select Case sLeft(sKey, 16)
                Case "ReferencedCharac"
                    Select Case sKey
                        Case "ReferencedCharacters"
                            Return sO & sKey.Replace("ReferencedCharacters", "Referenced Characters") & sC
                        Case "ReferencedCharacter"
                            Return sO & sKey.Replace("ReferencedCharacter", "Referenced Character") & sC
                        Case Else
                            Return sO & sKey.Replace("ReferencedCharacter", "Referenced Character ") & sC
                    End Select
                Case "ReferencedDirect"
                    Select Case sKey
                        Case "ReferencedDirections"
                            Return sO & sKey.Replace("ReferencedDirections", "Referenced Directions") & sC
                        Case "ReferencedDirection"
                            Return sO & sKey.Replace("ReferencedDirection", "Referenced Direction") & sC
                        Case Else
                            Return sO & sKey.Replace("ReferencedDirection", "Referenced Direction ") & sC
                    End Select
                Case "ReferencedObject"
                    Select Case sKey
                        Case "ReferencedObjects"
                            Return sO & sKey.Replace("ReferencedObjects", "Referenced Objects") & sC
                        Case "ReferencedObject"
                            Return sO & sKey.Replace("ReferencedObject", "Referenced Object") & sC
                        Case Else
                            Return sO & sKey.Replace("ReferencedObject", "Referenced Object ") & sC
                    End Select
                Case "ReferencedNumber"
                    Select Case sKey
                        Case "ReferencedNumbers"
                            Return sO & sKey.Replace("ReferencedNumbers", "Referenced Numbers") & sC
                        Case "ReferencedNumber"
                            Return sO & sKey.Replace("ReferencedNumber", "Referenced Number") & sC
                        Case Else
                            Return sO & sKey.Replace("ReferencedNumber", "Referenced Number ") & sC
                    End Select
                Case "ReferencedText"
                    Select Case sKey
                        Case "ReferencedText"
                            Return sO & sKey.Replace("ReferencedText", "Referenced Text") & sC
                        Case Else
                            Return sO & sKey.Replace("ReferencedText", "Referenced Text ") & sC
                    End Select
                Case "ReferencedLocati"
                    Select Case sKey
                        Case "ReferencedLocation"
                            Return sO & sKey.Replace("ReferencedLocation", "Referenced Location") & sC
                    End Select
                Case "ReferencedItem"
                    Return sO & sKey.Replace("ReferencedItem", "Referenced Item") & sC
            End Select
        ElseIf sKey.StartsWith("Parameter-") Then
            Return sO & sKey.Replace("Parameter-", "") & sC
        End If
        If sKey = ANYOBJECT Then Return sO & "Any Object" & sC
        If sKey = ANYCHARACTER Then Return sO & "Any Character" & sC
        If sKey = NOOBJECT Then Return sO & "No Object" & sC
        If sKey = THEFLOOR Then Return sO & IIf(bPCase, "The Floor", "the Floor").ToString & sC
        If sKey = THEPLAYER Then Return IIf(bPCase, sO & "The Player Character" & sC, "the Player character").ToString
        If sKey = CHARACTERPROPERNAME Then Return IIf(bPrefixItem, IIf(bPCase, "Property ", "property "), "").ToString & sQ & "Name" & sQ
        If sKey = PLAYERLOCATION Then Return IIf(bPCase, sO & "The Player's Location" & sC, "the Player's location").ToString

        If htblLocations.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Location ", "location "), "").ToString & sQ & htblLocations(sKey).ShortDescription.ToString & sQ
        If htblObjects.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Object ", "object "), "").ToString & sQ & htblObjects(sKey).FullName & sQ
        If htblTasks.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Task ", "task "), "").ToString & sQ & htblTasks(sKey).Description & sQ
        If htblEvents.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Event ", "event "), "").ToString & sQ & htblEvents(sKey).Description & sQ
        If htblCharacters.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Character ", "character "), "").ToString & sQ & htblCharacters(sKey).Name(, False, False) & sQ
        If htblGroups.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Group ", "group "), "").ToString & sQ & htblGroups(sKey).Name & sQ
        If htblVariables.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Variable ", "variable "), "").ToString & sQ & htblVariables(sKey).Name & sQ
        If htblALRs.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Text Override ", "text override "), "").ToString & sQ & htblALRs(sKey).OldText & sQ
        If htblHints.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Hint ", "hint "), "").ToString & sQ & htblHints(sKey).Question & sQ
        If htblAllProperties.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Property ", "property "), "").ToString & sQ & htblAllProperties(sKey).Description & sQ
        If htblUDFs.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "User Function ", "user function "), "").ToString & sQ & htblUDFs(sKey).Name & sQ
        If htblSynonyms.ContainsKey(sKey) Then Return IIf(bPrefixItem, IIf(bPCase, "Synonym ", "synonym "), "").ToString & sQ & htblSynonyms(sKey).CommonName & sQ

        Return sKey

    End Function

#End Region


    Public Enum TasksListEnum
        AllTasks
        GeneralTasks
        GeneralAndOverrideableSpecificTasks
        SpecificTasks
        SystemTasks
    End Enum
    ' Could probably speed things up by making these permanent, defining on Adventure load, for Runner anyway
    Friend Function Tasks(ByVal eTasksList As TasksListEnum) As TaskHashTable

        Dim htblSubTasks As New TaskHashTable

        Select Case eTasksList
            Case TasksListEnum.AllTasks
                htblSubTasks = htblTasks
            Case TasksListEnum.GeneralTasks
                For Each Task As clsTask In htblTasks.Values
                    If Task.TaskType = clsTask.TaskTypeEnum.General Then htblSubTasks.Add(Task, Task.Key)
                Next
            Case TasksListEnum.GeneralAndOverrideableSpecificTasks
                For Each Task As clsTask In htblTasks.Values
                    If Task.TaskType = clsTask.TaskTypeEnum.General Then htblSubTasks.Add(Task, Task.Key)
                    ' A specific task is overrideable if any of the specifics are unspecified
                    If Task.TaskType = clsTask.TaskTypeEnum.Specific Then
                        If Task.Specifics IsNot Nothing Then
                            For Each s As clsTask.Specific In Task.Specifics
                                If s.Keys.Count = 1 Then
                                    If s.Keys(0) = "" Then
                                        htblSubTasks.Add(Task, Task.Key)
                                        Exit For
                                    End If
                                End If
                            Next
                        End If
                    End If
                Next
            Case TasksListEnum.SpecificTasks
                For Each Task As clsTask In htblTasks.Values
                    If Task.TaskType = clsTask.TaskTypeEnum.Specific Then htblSubTasks.Add(Task, Task.Key)
                Next
            Case TasksListEnum.SystemTasks
                For Each Task As clsTask In htblTasks.Values
                    If Task.TaskType = clsTask.TaskTypeEnum.System Then htblSubTasks.Add(Task, Task.Key)
                Next
        End Select

        Return htblSubTasks

    End Function


    Friend Function Objects(ByVal sPropertyKey As String, Optional ByVal sPropertyValue As String = "") As ObjectHashTable

        Dim htblSubObjects As New ObjectHashTable

        For Each ob As clsObject In Adventure.htblObjects.Values
            If ob.HasProperty(sPropertyKey) Then
                If sPropertyValue = "" OrElse ob.GetPropertyValue(sPropertyKey) = sPropertyValue Then
                    htblSubObjects.Add(ob, ob.Key)
                End If
            End If
        Next

        Return htblSubObjects

    End Function


    Friend Property Introduction() As Description
        Get
            Return oIntroduction
        End Get
        Set(ByVal Value As Description)
            oIntroduction = Value
        End Set
    End Property

    Friend Property WinningText() As Description
        Get
            Return oWinningText
        End Get
        Set(ByVal Value As Description)
            oWinningText = Value
        End Set
    End Property

    Public Property Title() As String
        Get
            Return sTitle
        End Get
        Set(ByVal Value As String)
            sTitle = Value
        End Set
    End Property

    Public Property Author() As String
        Get
            Return sAuthor
        End Get
        Set(ByVal Value As String)
            sAuthor = Value
        End Set
    End Property

    Private sDefaultFontName As String = "Arial"
    Public Property DefaultFontName As String
        Get
            Return sDefaultFontName
        End Get
        Set(value As String)
            If value <> "" Then sDefaultFontName = value
        End Set
    End Property

    Public Property DeveloperDefaultBackgroundColour As Integer = DEFAULT_BACKGROUNDCOLOUR
    Public Property DeveloperDefaultInputColour As Integer = DEFAULT_INPUTCOLOUR
    Public Property DeveloperDefaultOutputColour As Integer = DEFAULT_OUTPUTCOLOUR
    Public Property DeveloperDefaultLinkColour As Integer = DEFAULT_LINKCOLOUR

    Private iDefaultFontSize As Integer = 12
    Public Property DefaultFontSize As Integer
        Get
            Return iDefaultFontSize
        End Get
        Set(value As Integer)
            If value >= 8 AndAlso value <= 36 Then iDefaultFontSize = value
        End Set
    End Property

    Dim _iWaitTurns As Integer = 3
    Public Property WaitTurns As Integer
        Get
            Return _iWaitTurns
        End Get
        Set(value As Integer)
            _iWaitTurns = Math.Max(value, 0)
        End Set
    End Property

    Public Property NotUnderstood() As String
        Get
            Return sNotUnderstood
        End Get
        Set(ByVal Value As String)
            sNotUnderstood = Value
        End Set
    End Property

    Public Property Filename() As String
        Get
            Return sFilename
        End Get
        Set(ByVal Value As String)
            sFilename = Value
        End Set
    End Property

    Public Property FullPath() As String
        Get
            Return sFullPath
        End Get
        Set(ByVal Value As String)
            sFullPath = Value
        End Set
    End Property

    Private bShowFirstRoom As Boolean = True
    Public Property ShowFirstRoom() As Boolean
        Get
            Return bShowFirstRoom
        End Get
        Set(ByVal value As Boolean)
            bShowFirstRoom = value
        End Set
    End Property

    Private bShowExits As Boolean = True
    Public Property ShowExits() As Boolean
        Get
            Return bShowExits
        End Get
        Set(ByVal value As Boolean)
            bShowExits = value
        End Set
    End Property

    Private bEnableMenu As Boolean = True
    Public Property EnableMenu As Boolean
        Get
            Return bEnableMenu
        End Get
        Set(value As Boolean)
            bEnableMenu = value
        End Set
    End Property

    Private bEnableDebugger As Boolean = True
    Public Property EnableDebugger As Boolean
        Get
            Return bEnableDebugger
        End Get
        Set(value As Boolean)
            bEnableDebugger = value
        End Set
    End Property

    Public Sub New()

        Title = "Untitled"
        Author = "Anonymous"
        sFilename = "untitled.taf"
        sGameFilename = ""
        With UserSession
            .sIt = ""
            .sThem = ""
            .sHim = ""
            .sHer = ""
        End With
        For i As Integer = 0 To 11
            Enabled(CType(i, EnabledOptionEnum)) = True
        Next
        Turns = 0
        dictAllItems = New ItemDictionary
        htblLocations = New LocationHashTable
        htblObjects = New ObjectHashTable
        htblTasks = New TaskHashTable
        htblEvents = New EventHashTable
        htblCharacters = New CharacterHashTable
        htblGroups = New GroupHashTable
        htblVariables = New VariableHashTable
        htblALRs = New ALRHashTable
        htblHints = New HintHashTable
        htblUDFs = New UDFHashTable
        htblAllProperties = New PropertyHashTable
        htblObjectProperties = New PropertyHashTable
        htblCharacterProperties = New PropertyHashTable
        htblLocationProperties = New PropertyHashTable
        htblSynonyms = New SynonymHashTable
        Introduction = New Description
        WinningText = New Description
        '#If Not www Then
        Map = New clsMap
        '#End If

        sDirectionsRE(DirectionsEnum.North) = "North/N"
        sDirectionsRE(DirectionsEnum.NorthEast) = "NorthEast/NE/North-East/N-E"
        sDirectionsRE(DirectionsEnum.East) = "East/E"
        sDirectionsRE(DirectionsEnum.SouthEast) = "SouthEast/SE/South-East/S-E"
        sDirectionsRE(DirectionsEnum.South) = "South/S"
        sDirectionsRE(DirectionsEnum.SouthWest) = "SouthWest/SW/South-West/S-W"
        sDirectionsRE(DirectionsEnum.West) = "West/W"
        sDirectionsRE(DirectionsEnum.NorthWest) = "NorthWest/NW/North-West/N-W"
        sDirectionsRE(DirectionsEnum.In) = "In/Inside"
        sDirectionsRE(DirectionsEnum.Out) = "Out/O/Outside"
        sDirectionsRE(DirectionsEnum.Up) = "Up/U"
        sDirectionsRE(DirectionsEnum.Down) = "Down/D"

        For Each eGen As clsCharacter.GenderEnum In New clsCharacter.GenderEnum() {clsCharacter.GenderEnum.Male, clsCharacter.GenderEnum.Female, clsCharacter.GenderEnum.Unknown}
            lCharMentionedThisTurn.Add(eGen, New Generic.List(Of String))
        Next

    End Sub

    Friend Enum EnabledOptionEnum
        ShowExits
        EightPointCompass
        BattleSystem
        Sound
        Graphics
        UserStatusBox
        Score
        ControlPanel
        Debugger
        Map
        AutoComplete
        MouseClicks
    End Enum

    Private bEnabled(11) As Boolean
    Friend Property Enabled(ByVal eOption As EnabledOptionEnum) As Boolean
        Get
            Return bEnabled(eOption)
        End Get
        Set(ByVal Value As Boolean)
            bEnabled(eOption) = Value
        End Set
    End Property





    Friend ReadOnly Property AllKeys() As List(Of String) ' StringArrayList
        Get
            Dim salKeys As New List(Of String) 'stringArrayList

            For Each sKey As String In Me.htblALRs.Keys
                salKeys.Add(sKey)
            Next
            For Each sKey As String In Me.htblCharacters.Keys
                salKeys.Add(sKey)
            Next
            For Each sKey As String In Me.htblEvents.Keys
                salKeys.Add(sKey)
            Next
            For Each sKey As String In Me.htblGroups.Keys
                salKeys.Add(sKey)
            Next
            For Each sKey As String In Me.htblHints.Keys
                salKeys.Add(sKey)
            Next
            For Each skey As String In Me.htblLocations.Keys
                salKeys.Add(skey)
            Next
            For Each skey As String In Me.htblObjects.Keys
                salKeys.Add(skey)
            Next
            For Each sKey As String In Me.htblAllProperties.Keys
                salKeys.Add(sKey)
            Next
            For Each sKey As String In Me.htblTasks.Keys
                salKeys.Add(sKey)
            Next
            For Each sKey As String In Me.htblVariables.Keys
                salKeys.Add(sKey)
            Next
            For Each sKey As String In Me.htblUDFs.Keys
                salKeys.Add(sKey)
            Next

            Return salKeys

        End Get
    End Property

End Class
