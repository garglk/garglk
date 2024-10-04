' Everything specific to the user's session that isn't stored in Adventure needs to go here

Imports System.Xml

Imports FrankenDrift.Glue
Imports FrankenDrift.Glue.Util

Public Class RunnerSession

    Public sIt, sHim, sHer, sThem As String
    Public dtDebug As DateTime
    Public iDebugIndent As Integer
    Public bNoDebug As Boolean
    Public iPrepProgress As Integer
    Public States As New StateStack
    Public iMarginWidth As Integer
    Public bEXE As Boolean = False
    Private listTaskKeys As New Generic.List(Of TaskKey)
    Friend iMatchedTaskCommand As Integer
    Friend dictMacros As New Generic.Dictionary(Of String, clsMacro)
    Friend bSystemTask As Boolean = False
    Public salCommands As New StringArrayList
    Public bShowShortLocations As Boolean = True
    Friend iPreviousOffset As Integer
    Public sGameFolder As String
    Public bGraphics As Boolean = True
    Public WithEvents Map As FrankenDrift.Glue.Map
    Friend bUseDefaultFont As Boolean = False
    Friend sTurnOutput As String = ""
    Friend bRequiresRestoreLayout As Boolean = False
    Friend sRememberedVerb As String = ""
    Friend bTestingOutput As Boolean = False

    Public Property bSound As Boolean = True

    Private Class TaskKey
        Implements IComparable

        Public sTaskKey As String
        Public iPriority As Integer

        Public Function CompareTo(ByVal obj As Object) As Integer Implements System.IComparable.CompareTo
            Return Me.iPriority.CompareTo(CType(obj, TaskKey).iPriority)
        End Function
    End Class

    Friend NewReferences() As clsNewReference ' We should really keep a NewReferences collection PER parent task.  So if we execute tasks within our actions, they have their own References set!!!

    Private htblResponsesPass As New OrderedHashTable
    Private htblResponsesFail As New OrderedHashTable
    Public htblCompleteableGeneralTasks As New TaskHashTable
    Public htblSecondChanceTasks As New TaskHashTable ' If we don't get a winner first time round, try again, matching against objects/characters that don't exist
    Public htblVisibleObjects As New ObjectHashTable
    Public htblSeenObjects As New ObjectHashTable
    Public sInput, sLastProperInput As String
    Friend iRestNum As Integer
    Private sAmbTask As String
    Public sOutputText As String
    Public sRestrictionText, sRestrictionTextMulti As String

    Friend Property sRouteError As String

    Private ValidRefs As Hashtable

    Friend PronounKeys As New PronounInfoList
    Private Class SortByLength
        Inherits Generic.Comparer(Of String)

        Public Overrides Function Compare(ByVal x As String, ByVal y As String) As Integer
            Return y.Length.CompareTo(x.Length)
        End Function
    End Class


    Public Sub SaveMacros()

        Try
            Dim sMacrosFile As String = DataPath(True) & "ADRIFTMacros.xml"

            Dim xmlWriter As New System.Xml.XmlTextWriter(sMacrosFile, System.Text.Encoding.UTF8)
            With xmlWriter
                .Indentation = 4
                .IndentChar = " "c
                .Formatting = Formatting.Indented

                .WriteStartDocument()
                .WriteComment("File created by ADRIFT v" & Application.ProductVersion)

                .WriteStartElement("Macros")

                For Each sTitle As String In dictMacros.Keys
                    .WriteStartElement("Macro")

                    Dim m As clsMacro = dictMacros(sTitle)

                    .WriteElementString("Key", m.Key)
                    .WriteElementString("Title", m.Title)
                    .WriteElementString("Shared", m.Shared.ToString)
                    .WriteElementString("IFID", m.IFID)
                    .WriteElementString("Commands", m.Commands)

                    .WriteEndElement() ' Macro
                Next

                .WriteEndElement() ' Macros

                .WriteEndDocument()

                .Flush()
                .Close()

            End With
        Catch ex As Exception
            ErrMsg("Error saving macros", ex)
        End Try

    End Sub


    ' Load Macros from file
    Public Sub LoadMacros()

        Try
            Dim sMacrosFile As String = DataPath(True) & "ADRIFTMacros.xml"
            dictMacros.Clear()

            If IO.File.Exists(sMacrosFile) Then
                Dim xmlDoc As New System.Xml.XmlDocument
                xmlDoc.Load(sMacrosFile)
                Dim bFirst As Boolean = True

                For Each nod As System.Xml.XmlNode In xmlDoc.SelectNodes("/Macros/Macro")

                    Dim sTitle As String = nod.SelectSingleNode("Title").InnerText
                    Dim sCommands As String = nod.SelectSingleNode("Commands").InnerText
                    Dim sKey As String = nod.SelectSingleNode("Key").InnerText

                    If sKey <> "" AndAlso sTitle <> "" AndAlso sCommands <> "" Then
                        Dim macro As New clsMacro(sKey)
                        macro.Title = sTitle
                        macro.Commands = sCommands

                        If nod.SelectSingleNode("Shared") IsNot Nothing Then macro.Shared = SafeBool(nod.SelectSingleNode("Shared").InnerText)
                        If nod.SelectSingleNode("IFID") IsNot Nothing Then macro.IFID = SafeString(nod.SelectSingleNode("IFID").InnerText)
                        dictMacros.Add(macro.Key, macro)
                    End If
                Next
            End If

        Catch ex As Exception
            ErrMsg("Error loading macros", ex)
        End Try

    End Sub


    Public Function OpenAdventure(ByVal sFilename As String, Optional ByVal bSilentError As Boolean = False) As Boolean

        Adventure = New clsAdventure

        States.Clear()
        salCommands.Clear()
#If Not Adravalon Then
        salCommands.Add(fRunner.txtInput.Text)
#Else
        salCommands.Add("")
#End If
        iPreviousOffset = 0
        Dim eFileType As FileIO.FileTypeEnum = FileTypeEnum.TextAdventure_TAF
        If sFilename.ToLower.EndsWith(".blorb") Then eFileType = FileTypeEnum.Blorb
        If sFilename.ToLower.EndsWith(".exe") Then eFileType = FileTypeEnum.Exe

        If LoadFile(sFilename, eFileType, LoadWhatEnum.All, False, , , , bSilentError) Then
            For eDirection As DirectionsEnum = DirectionsEnum.North To DirectionsEnum.NorthWest
                Adventure.sDirectionsRE(eDirection) = Adventure.sDirectionsRE(eDirection).ToLower
            Next
            Glue.SetGameName(Adventure.Title)

            'Sort tasks by priority
            listTaskKeys.Clear()
            For Each tas As clsTask In Adventure.htblTasks.Values
                Dim tk As New TaskKey
                tk.sTaskKey = tas.Key
                tk.iPriority = tas.Priority
                listTaskKeys.Add(tk)
            Next
            listTaskKeys.Sort()

            Adventure.eGameState = clsAction.EndGameEnum.Running

            ' Initialise any array values
            For Each v As clsVariable In Adventure.htblVariables.Values
                If v.Length > 1 Then
                    Dim sInitialValue() As String = v.StringValue.Split(Chr(10))
                    If sInitialValue.Length = v.Length Then
                        Dim i As Integer = 1
                        For Each sValue As String In sInitialValue
                            If v.Type = clsVariable.VariableTypeEnum.Numeric Then
                                v.IntValue(i) = SafeInt(sValue)
                            Else
                                v.StringValue(i) = sValue.Replace(Chr(13), "")
                            End If
                            i += 1
                        Next
                    End If
                End If
            Next

            For Each t As clsTask In Adventure.htblTasks.Values
                For i As Integer = 0 To t.arlCommands.Count - 1
                    t.arlCommands(i) = CorrectCommand(t.arlCommands(i))
                Next
                If t.TaskType = clsTask.TaskTypeEnum.System AndAlso t.RunImmediately Then
                    AttemptToExecuteTask(t.Key, True)
                End If
            Next

            UpdateStatusBar()
            InitialiseInputBox()
            fRunner.SetBackgroundColour()

            Adventure.Player.HasSeenLocation(Adventure.Player.Location.LocationKey) = True
            UserSession.Map.RecalculateNode(Adventure.Map.FindNode(Adventure.Player.Location.LocationKey))
            UserSession.Map.SelectNode(Adventure.Player.Location.LocationKey)
            fRunner.txtOutput.Clear()
            Display("<c>" & Adventure.Title & "</c>" & vbCrLf, True)
            Display(Adventure.Introduction.ToString, True)

            If Adventure.ShowFirstRoom AndAlso Adventure.htblLocations.ContainsKey(Adventure.Player.Location.LocationKey) Then Display(vbCrLf & pSpace(Adventure.htblLocations(Adventure.Player.Location.LocationKey).ViewLocation), True)

            For Each e As clsEvent In Adventure.htblEvents.Values
                Select Case e.WhenStart
                    Case clsEvent.WhenStartEnum.AfterATask
                        e.Status = clsEvent.StatusEnum.NotYetStarted
                    Case clsEvent.WhenStartEnum.BetweenXandYTurns
                        e.Status = clsEvent.StatusEnum.CountingDownToStart
                        e.TimerToEndOfEvent = e.StartDelay.Value + e.Length.Value
                    Case clsEvent.WhenStartEnum.Immediately
                        e.Start(True)
                End Select
            Next

            ' Needs to be a separate loop in case a later event runs a task that starts an earlier event
            For Each e As clsEvent In Adventure.htblEvents.Values
                e.bJustStarted = False
            Next

            For Each c As clsCharacter In Adventure.htblCharacters.Values
                For Each w As clsWalk In c.arlWalks
                    If w.StartActive Then w.Start(True)
                Next
                For Each w As clsWalk In c.arlWalks
                    w.bJustStarted = False
                Next

                ' Sort our topics by descending length
                Dim topickeys As New Generic.SortedDictionary(Of String, Generic.List(Of String))(New SortByLength)
                For Each t As clsTopic In c.htblTopics.Values
                    If t.bCommand Then t.Keywords = CorrectCommand(t.Keywords)
                    If Not topickeys.ContainsKey(t.Keywords) Then topickeys.Add(t.Keywords, New Generic.List(Of String))
                    topickeys(t.Keywords).Add(t.Key)
                Next
                Dim htblTopicsNew As New TopicHashTable
                For Each sTopic As String In topickeys.Keys
                    For Each sKey As String In topickeys(sTopic)
                        htblTopicsNew.Add(c.htblTopics(sKey))
                    Next
                Next
                c.htblTopics = htblTopicsNew
            Next

            Display(vbCrLf & vbCrLf, True)

            For Each task As clsTask In Adventure.htblTasks.Values
                ReDim task.NewReferences(task.References.Count - 1)
            Next

            If Adventure.htblLocations.Count = 0 Then
                ErrMsg("This adventure has no locations.  Cannot continue.")
                Return False
            End If

            PrepareForNextTurn()

            Return True
        Else
            Return False
        End If

    End Function


    Public Sub InitialiseInputBox(Optional ByVal sCursor As String = "Ø")
        Glue.InitInput()
    End Sub

    Public bDisplaying As Boolean = False ' In case any output is once only - don't want it to trigger when we're just testing the text
    Public Sub Display(ByVal sText As String, Optional ByVal bCommit As Boolean = False, Optional ByVal bAllowALR As Boolean = True, Optional ByVal bRecord As Boolean = True)

        bDisplaying = True
        Dim bAllowPSpace As Boolean = True

        If Adventure IsNot Nothing AndAlso Adventure.dVersion < 5 Then
            ' ViewRoom function used to always start at beginning of line, so no pspace
            If sText.StartsWith("%DisplayLocation[") Then bAllowPSpace = False
        End If

        If Adventure IsNot Nothing AndAlso bAllowALR Then sText = ReplaceALRs(sText)
        If bAllowPSpace AndAlso sText <> vbCrLf AndAlso sText <> "" Then
            sOutputText = pSpace(sOutputText) & sText
        Else
            sOutputText = sOutputText & sText
        End If

        If bCommit Then
            ' Ensure text doesn't get output more than once...
            Dim theText As String = sOutputText
            sOutputText = ""
            Source2HTML(theText, fRunner.txtOutput, False)

            If bRecord Then
                While theText.EndsWith(vbCrLf)
                    theText = theText.Substring(0, theText.Length - 2)
                End While
                sTurnOutput &= theText
            End If
        End If

        bDisplaying = False
    End Sub

    Public Sub LoadDefaults()
        CreatePlayer()
        CreateRequiredProperties()
    End Sub


    Private Sub CreatePlayer()
        Dim Player As New clsCharacter
        With Player
            .ProperName = "Player"
            .CharacterType = clsCharacter.CharacterTypeEnum.Player
            .Key = "Player"
        End With
        Adventure.htblCharacters.Add(Player, Player.Key)
    End Sub

    Private Sub CreateRequiredProperties()

        Dim pStaticDynamic As New clsProperty
        With pStaticDynamic
            .Key = "StaticOrDynamic"
            .Description = "Object type"
            .Type = clsProperty.PropertyTypeEnum.StateList
            .arlStates.Add("Static")
            .arlStates.Add("Dynamic")
            .Mandatory = True
        End With
        Adventure.htblAllProperties.Add(pStaticDynamic)

        Dim pSurface As New clsProperty
        With pSurface
            .Key = "Surface"
            .Description = "This object has a surface"
            .Type = clsProperty.PropertyTypeEnum.SelectionOnly
        End With
        Adventure.htblAllProperties.Add(pSurface)

        Dim pSurfaceHold As New clsProperty
        With pSurfaceHold
            .Key = "SurfaceHold"
            .Description = "Surface can hold"
            .Type = clsProperty.PropertyTypeEnum.Integer
            .DependentKey = "Surface"
        End With
        Adventure.htblAllProperties.Add(pSurfaceHold)

        Dim pReadable As New clsProperty
        With pReadable
            .Key = "Readable"
            .Description = "This object is readable"
            .Type = clsProperty.PropertyTypeEnum.SelectionOnly
        End With
        Adventure.htblAllProperties.Add(pReadable)

        Dim pReadText As New clsProperty
        With pReadText
            .Key = "ReadText"
            .Description = "Description when read"
            .Type = clsProperty.PropertyTypeEnum.Text
            .DependentKey = "Readable"
        End With
        Adventure.htblAllProperties.Add(pReadText)

        Dim pWearable As New clsProperty
        With pWearable
            .Key = "Wearable"
            .Description = "Wearable"
            .Type = clsProperty.PropertyTypeEnum.SelectionOnly
            .DependentKey = "StaticOrDynamic"
            .DependentValue = "Dynamic"
        End With
        Adventure.htblAllProperties.Add(pWearable)

        Dim pContainer As New clsProperty
        With pContainer
            .Key = "Container"
            .Description = "This object is a container"
            .Type = clsProperty.PropertyTypeEnum.SelectionOnly
        End With
        Adventure.htblAllProperties.Add(pContainer)

        Dim pContainerHold As New clsProperty
        With pContainerHold
            .Key = "ContainerHold"
            .Description = "Container can hold"
            .Type = clsProperty.PropertyTypeEnum.Integer
            .DependentKey = "Container"
        End With
        Adventure.htblAllProperties.Add(pContainerHold)

        Dim pOpenable As New clsProperty
        With pOpenable
            .Key = "Openable"
            .Description = "Object can be opened and closed"
            .Type = clsProperty.PropertyTypeEnum.StateList
            .arlStates.Add("Open")
            .arlStates.Add("Closed")
        End With
        Adventure.htblAllProperties.Add(pOpenable)

        Dim pLockable As New clsProperty
        With pLockable
            .Key = "Lockable"
            .Description = "And is lockable, with key"
            .Type = clsProperty.PropertyTypeEnum.ObjectKey
            .DependentKey = "Openable"
        End With
        Adventure.htblAllProperties.Add(pLockable)

        Dim pSittable As New clsProperty
        With pSittable
            .Key = "Sittable"
            .Description = "Characters can sit on this object"
            .Type = clsProperty.PropertyTypeEnum.SelectionOnly
        End With
        Adventure.htblAllProperties.Add(pSittable)

        Dim pStandable As New clsProperty
        With pStandable
            .Key = "Standable"
            .Description = "Characters can stand on this object"
            .Type = clsProperty.PropertyTypeEnum.SelectionOnly
        End With
        Adventure.htblAllProperties.Add(pStandable)

        Dim pLieable As New clsProperty
        With pLieable
            .Key = "Lieable"
            .Description = "Characters can lie on this object"
            .Type = clsProperty.PropertyTypeEnum.SelectionOnly
        End With
        Adventure.htblAllProperties.Add(pLieable)

        Dim pEdible As New clsProperty
        With pEdible
            .Key = "Edible"
            .Description = "Object is edible"
            .Type = clsProperty.PropertyTypeEnum.SelectionOnly
            .DependentKey = "StaticOrDynamic"
            .DependentValue = "Dynamic"
        End With
        Adventure.htblAllProperties.Add(pEdible)

    End Sub


    Public Sub Process(ByVal sCommand As String)

        If Not Adventure Is Nothing Then
            iPreviousOffset = fRunner.txtOutput.TextLength
            If iPrepProgress = 0 Then iPrepProgress = 1
            bNoDebug = False
            dtDebug = Now
            iDebugIndent = 0
            bSystemTask = False
            Dim dtStart As Date = Now
            While iPrepProgress <> 2 AndAlso Now < dtStart.AddSeconds(10) ' Wait for the other thread to finish
                Application.DoEvents()
                Threading.Thread.Sleep(1)
            End While
            sInput = sCommand

            If EvaluateInput(0, False) = "***SYSTEM***" Then
                sOutputText = ""
                Exit Sub
            End If
            bNoDebug = True

            CheckEndOfGame()
            If Adventure.eGameState <> clsAction.EndGameEnum.Running Then Exit Sub

            iPrepProgress = 0
            PrepareForNextTurn()

            Adventure.Player.DoWalk()
        End If
    End Sub


    Private Sub CheckEndOfGame()
        If Not Adventure.bDisplayedWinLose Then
            Select Case Adventure.eGameState
                Case clsAction.EndGameEnum.Win
                    Display("<center><c><b>*** You have won ***</b></c></center>" & vbCrLf, True)
                Case clsAction.EndGameEnum.Lose
                    Display("<center><c><b>*** You have lost ***</b></c></center>" & vbCrLf, True)
                Case clsAction.EndGameEnum.Neutral
                    ' Don't display anything
                Case clsAction.EndGameEnum.Running
                    ' Continue
            End Select
            If Adventure.eGameState <> clsAction.EndGameEnum.Running Then States.RecordState()
        End If
        If Adventure.eGameState <> clsAction.EndGameEnum.Running Then
            Adventure.Player.WalkTo = ""
            If Not Adventure.bDisplayedWinLose AndAlso Adventure.WinningText.ToString(True) <> "" Then
                Display(Adventure.WinningText.ToString & vbCrLf)
            End If
            If Adventure.MaxScore > 0 AndAlso Not Adventure.bDisplayedWinLose Then
                Display("In that game you scored " & Adventure.Score & " out of a possible " & Adventure.MaxScore & ", in " & Adventure.Turns & " turns." & vbCrLf & vbCrLf, True)
            End If
            Display("Would you like to <c>restart</c>, <c>restore</c> a saved game, <c>quit</c> or <c>undo</c> the last command?" & vbCrLf & vbCrLf, True)
            Adventure.bDisplayedWinLose = True
        End If
    End Sub


    Private Function KeyListsMatch(ByVal salSpecifics As StringArrayList, ByVal salReferences As ArrayList, ByVal bMultiple As Boolean) As Boolean
        If salSpecifics.Count = salReferences.Count Then
            For Each sSpecific As String In salSpecifics
                Dim bFound As Boolean = False
                For Each salRef As StringArrayList In salReferences
                    If salRef(0) = sSpecific Then
                        bFound = True
                        Exit For
                    End If
                Next
                If Not bFound Then Return False
            Next
        Else
            Return False
        End If

        Return True
    End Function


    ' Ensures every Reference has at least one object assigned to it
    Private Function AllRefsHaveAtLeastOne() As Boolean
        If NewReferences Is Nothing Then Return False

        For iRef As Integer = 0 To NewReferences.Length - 1
            With NewReferences(iRef)
                If .Items.Count > 0 Then
                    For Each itm As clsSingleItem In .Items
                        If itm.MatchingPossibilities.Count = 0 Then Return False
                    Next
                Else
                    Return False
                End If
            End With
        Next
        Return True
    End Function


    ' If we have multiple commands, and we are matching on a different command, make sure we get the right ref
    ' E.g. give %object% to %character%
    '      give %character% %object%
    Private Function GetAlternateRef(ByVal task As clsTask, ByVal iRef As Integer) As Integer
        While task.TaskType = clsTask.TaskTypeEnum.Specific AndAlso task.Parent <> ""
            task = Adventure.htblTasks(task.Parent)
        End While

        Dim sMatchingRef As String = task.References(iRef)
        Dim iMatch As Integer = 0

        For Each sOrigRef As String In task.RefsInCommand(task.TaskCommand(task, False, 0))
            If sOrigRef = sMatchingRef Then Return iMatch
            iMatch += 1
        Next
        Return iRef
    End Function

    ' Specifics are always defined in the order of the first command in the task
    ' We may be matching on a different command, in which case Specifics will be
    ' in a different order from the References
    '
    Private Function RefsMatchSpecifics(ByVal tasChild As clsTask) As Boolean
        Dim aSpecifics() As clsTask.Specific = tasChild.Specifics
        Dim alSpecs As New ArrayList
        Dim salRefs As New StringArrayList

        ' See if we have all the Specifics we need in the References
        If NewReferences IsNot Nothing AndAlso aSpecifics.Length = NewReferences.Length Then
            For iSpec As Integer = 0 To aSpecifics.Length - 1
                ' Make sure References contains all Specifics
                With aSpecifics(iSpec)
                    For Each sKey As String In .Keys ' We must find all of these in order for the task to match
                        Dim bKeyFoundInRefs As Boolean = False
                        If sKey = "" Then ' i.e. match any object/character etc...
                            bKeyFoundInRefs = True
                        Else
                            If sKey.Contains("%") Then sKey = ReplaceFunctions(sKey)

                            Dim iReference As Integer = iSpec
                            ' TODO - If this is matching on a second command in the task where the refs are the other way around, it fails to match the specifics
                            ' This is because Specific tasks always match on the first General task command
                            If UserSession.iMatchedTaskCommand > 0 Then
                                iReference = GetAlternateRef(tasChild, iReference)
                            End If

                            ' Grab the correct Reference
                            Dim NewRef As clsNewReference = NewReferences(iReference)
                            For Each Ref As clsNewReference In NewReferences
                                If Ref.Index = iSpec Then
                                    NewRef = Ref
                                    Exit For
                                End If
                            Next

                            For iRef As Integer = NewRef.Items.Count - 1 To 0 Step -1
                                If NewRef.Items(iRef).MatchingPossibilities(0).ToLower = sKey.ToLower Then
                                    bKeyFoundInRefs = True
                                    alSpecs.Add(iSpec)
                                    salRefs.Add(sKey)
                                    Exit For
                                End If
                            Next
                        End If
                        If Not bKeyFoundInRefs Then Return False
                    Next
                End With
            Next
        Else
            ' Not matching same amount
            Return False
        End If
        Return True
    End Function


    Private Function RefsMatchSpecificsOld(ByVal tasChild As clsTask) As Boolean
        Dim aSpecifics() As clsTask.Specific = tasChild.Specifics
        Dim alSpecs As New ArrayList
        Dim salRefs As New StringArrayList

        ' See if we have all the Specifics we need in the References
        If NewReferences IsNot Nothing AndAlso aSpecifics.Length = NewReferences.Length Then
            For iSpec As Integer = 0 To aSpecifics.Length - 1
                ' Make sure References contains all Specifics
                With aSpecifics(iSpec)
                    For Each sKey As String In .Keys ' We must find all of these in order for the task to match
                        Dim bKeyFoundInRefs As Boolean = False
                        If sKey = "" Then
                            bKeyFoundInRefs = True
                        Else
                            If sKey = "%Player%" Then sKey = Adventure.Player.Key
                            For iRef As Integer = NewReferences(iSpec).Items.Count - 1 To 0 Step -1
                                If NewReferences(iSpec).Items(iRef).MatchingPossibilities(0) = sKey Then
                                    bKeyFoundInRefs = True
                                    alSpecs.Add(iSpec)
                                    salRefs.Add(sKey)
                                    Exit For
                                End If
                            Next
                        End If
                        If Not bKeyFoundInRefs Then Return False
                    Next
                End With
            Next
        Else
            ' Not matching same amount
            Return False
        End If
        Return True
    End Function


    Friend Sub DebugPrint(ByVal eItemType As ItemEnum, ByVal sKey As String, ByVal eDetailLevel As DebugDetailLevelEnum, ByVal sMessage As String, Optional ByVal bNewLine As Boolean = True)
    End Sub


    Private Function ExecuteSubTasks(ByVal sTaskKey As String, ByVal bCalledFromEvent As Boolean, ByVal bChildTask As Boolean, ByVal InReferences() As clsNewReference, ByVal iRefIndex As Integer, ByVal sReferenceKeys() As String, ByVal sReferenceCommands() As String, ByRef bTaskHasOutputNew As Boolean, ByVal bPassingOnly As Boolean) As Boolean
        If iRefIndex < InReferences.Length Then
            If InReferences(iRefIndex) Is Nothing OrElse InReferences(iRefIndex).Items Is Nothing OrElse InReferences(iRefIndex).Items.Count = 0 Then
                sReferenceKeys(iRefIndex) = ""
                sReferenceCommands(iRefIndex) = ""
                If ExecuteSubTasks(sTaskKey, bCalledFromEvent, bChildTask, InReferences, iRefIndex + 1, sReferenceKeys, sReferenceCommands, bTaskHasOutputNew, bPassingOnly) Then Return True
            Else
                For iItem As Integer = 0 To InReferences(iRefIndex).Items.Count - 1
                    If InReferences(iRefIndex).Items(iItem).MatchingPossibilities.Count > 0 Then
                        sReferenceKeys(iRefIndex) = InReferences(iRefIndex).Items(iItem).MatchingPossibilities(0)
                    Else
                        TODO("Check that this is intended...") ' Assume it's ok to leave this reference key as Nothing as there was no match
                    End If
                    sReferenceCommands(iRefIndex) = InReferences(iRefIndex).Items(iItem).sCommandReference
                    If ExecuteSubTasks(sTaskKey, bCalledFromEvent, bChildTask, InReferences, iRefIndex + 1, sReferenceKeys, sReferenceCommands, bTaskHasOutputNew, bPassingOnly) Then ExecuteSubTasks = True
                Next
            End If
        Else
            ' Ok, let's execute 'em
            If AttemptToExecuteSubTask(sTaskKey, sReferenceKeys, bCalledFromEvent, bChildTask, sReferenceCommands, bTaskHasOutputNew, bPassingOnly) Then Return True
        End If
    End Function


    ' EvaluateResponses - Should we replace functions for anything output by this task?  We do this so that e.g. ViewRoom will be at the time the task was executed
    ' bPassingOnly - Only do anything with the task if it passes.  Means we already have a failing task with output, so don't need another
    ' bAssignSpecificRefs - If we are attempting to run a specific task(e.g. from an event) then set the refs to those defined in the task
    '
    Public Function AttemptToExecuteTask(ByVal sTaskKey As String, Optional ByVal bCalledFromEvent As Boolean = False, Optional ByVal bSkipRestrictionsTest As Boolean = False, Optional ByVal bChildTask As Boolean = False, Optional ByRef bContinue As Boolean = False, Optional ByRef bTaskHasOutputNew As Boolean = False, Optional ByVal bEvaluateResponses As Boolean = False, Optional bPassingOnly As Boolean = False, Optional bAssignSpecificRefs As Boolean = False) As Boolean
        ' E.g. if our task is "get red ball and blue ball from box", then subtasks are
        ' "get red ball from box" and
        ' "get blue ball from box"

        Dim task As clsTask = Adventure.htblTasks(sTaskKey)

        If task.Completed AndAlso Not task.Repeatable Then Return False
        DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.Low, "Attempting to execute task " & task.Description & "...")

        Dim htblResponsesPassTemp As OrderedHashTable = Nothing
        Dim htblResponsesFailTemp As OrderedHashTable = Nothing
        If Not bChildTask Then
            If htblResponsesPass.Count > 0 Then htblResponsesPassTemp = htblResponsesPass.Clone
            If htblResponsesFail.Count > 0 Then htblResponsesFailTemp = htblResponsesFail.Clone
            htblResponsesPass.Clear() ' Will this cause a problem, or do we just need to call it before events run tasks for example?
            htblResponsesFail.Clear()
        End If

        ' This is really just in case anyone tries to run a specific task from an event - we need to know what the refs should be
        If task.TaskType = clsTask.TaskTypeEnum.Specific AndAlso bAssignSpecificRefs Then
            ReDim NewReferences(task.Specifics.Length - 1)
            For i As Integer = 0 To NewReferences.Length - 1
                NewReferences(i) = New clsNewReference(task.Specifics(i).Type)
                With NewReferences(i)
                    For Each sKey As String In task.Specifics(i).Keys
                        .Items.Add(New clsSingleItem(sKey))
                    Next
                End With
            Next
        End If

        Dim bPass As Boolean = False
        Dim InReferences() As clsNewReference = task.CopyNewRefs(NewReferences)
        If InReferences Is Nothing Then ReDim InReferences(-1)

        ' If we mention any characters on the command line, add them to the mentioned characters list (so we get "the" char rather than "a" char)
        For Each ref As clsNewReference In InReferences
            If ref IsNot Nothing Then
                If ref.ReferenceType = ReferencesType.Character Then
                    For Each itm As clsSingleItem In ref.Items
                        If itm.MatchingPossibilities.Count = 1 Then
                            If Adventure.htblCharacters.ContainsKey(itm.MatchingPossibilities(0)) Then
                                Dim ch As clsCharacter = Adventure.htblCharacters(itm.MatchingPossibilities(0))
                                Adventure.lCharMentionedThisTurn(ch.Gender).Add(ch.Key)
                            End If
                        End If
                    Next
                End If
            End If
        Next

        Dim sReferenceKeys(InReferences.Length - 1) As String
        Dim sReferenceCommands(InReferences.Length - 1) As String
        bPass = ExecuteSubTasks(sTaskKey, bCalledFromEvent, bChildTask, InReferences, 0, sReferenceKeys, sReferenceCommands, bTaskHasOutputNew, bPassingOnly)

        If bPassingOnly AndAlso Not bPass Then Return False

        If Not bChildTask OrElse bEvaluateResponses Then
            ' Create htblResponses from everything in htblResponsesPass, plus anything from ResponsesFail that isn't in ResponsesPass
            Dim htblResponses As New OrderedHashTable
            For Each sKey As String In htblResponsesPass.OrderedKeys
                htblResponses.Add(sKey, htblResponsesPass(sKey))
            Next

            If htblResponsesPass.Count = 0 AndAlso htblResponsesFail.Count >= 0 AndAlso task.FailOverride.ToString <> "" AndAlso ContainsWord(sInput, "all") Then
                Display(task.FailOverride.ToString)
            Else

                ' Get A, B, C from D
                ' Pass: You pick up A, B from D
                ' Fail: You can't get A from D
                ' Fail: B, C is too heavy to pick up
                ' Need: You pick up A, B from D.  C is too heavy to pick up.

                ' TODO - We could have responses from different tasks here.  We need to only match references from the same parent task
                ' So, for example, if we have an action that executes a different task with a different number of references, the different
                ' tasks should not cancel each other out.
                ' E.g. Stand on current object.  Has no references, but executes task as action which has a parameter

                ' So look at every reference combination in each failure message.  If we don't have that combination in our pass set then add it
                For Each sFailMessage As String In htblResponsesFail.OrderedKeys
                    Dim refsFail() As clsNewReference = CType(htblResponsesFail(sFailMessage), clsNewReference())
                    Dim bAllMatch As Boolean = False

                    For Each sPassMessage As String In htblResponsesPass.OrderedKeys
                        Dim refsPass() As clsNewReference = CType(htblResponsesPass(sPassMessage), clsNewReference())
                        bAllMatch = True

                        For iRef As Integer = 0 To refsFail.Length - 1
                            Dim refFail As clsNewReference = refsFail(iRef)
                            If refFail Is Nothing Then ' I think this is where our task could be executing a different task, with different references...
                                bAllMatch = False
                            Else
                                For iItm As Integer = 0 To refFail.Items.Count - 1
                                    Dim itmFail As clsSingleItem = refFail.Items(iItm)
                                    ' There should only be one matching possibility here, so no need to iterate them

                                    ' resPass(iRef) Is Nothing - think this may also be different task/different refs issue...
                                    ' Should only do this if it is for the same parent task.  If we execute a sub-task, that should become the parent
                                    If refsPass.Length <= iRef OrElse refsPass(iRef) Is Nothing OrElse refsPass(iRef).Items.Count <= iItm OrElse refsPass(iRef).Items(iItm).MatchingPossibilities(0) <> itmFail.MatchingPossibilities(0) Then
                                        ' This fail is different from this pass
                                        bAllMatch = False
                                        GoTo NextPassMessage
                                    End If

                                Next iItm
                            End If
                        Next iRef
NextPassMessage:
                        If bAllMatch Then Exit For
                    Next sPassMessage

                    If Not bAllMatch Then
                        ' We've found a failure message that didn't match any pass messages
                        If htblResponses.ContainsKey(sFailMessage) Then
                            ' Need to add refs for this message into the message
                            ' TODO
                            sFailMessage = sFailMessage
                        Else
                            ' Need to add this message
                            htblResponses.Add(sFailMessage, htblResponsesFail(sFailMessage))
                        End If
                    End If

                Next sFailMessage

                For Each sMessage As String In htblResponses.OrderedKeys
                    Dim refs As clsNewReference() = CType(htblResponses(sMessage), clsNewReference())

                    NewReferences = refs
                    Display(sMessage)
                Next

            End If

            If bEvaluateResponses Then
                htblResponsesPass.Clear()
                htblResponsesFail.Clear()
            End If
        End If

        If bPass Then
            ' Check any walks/events to see if anything triggers on this task completing
            For Each c As clsCharacter In Adventure.htblCharacters.Values
                For Each w As clsWalk In c.arlWalks
                    For Each ctrl As EventOrWalkControl In w.WalkControls
                        If ctrl.eCompleteOrNot = EventOrWalkControl.CompleteOrNotEnum.Completion AndAlso ctrl.sTaskKey = task.Key Then
                            ' If a child task of the current task has affected the walk control, ignore this as a trigger
                            If w.sTriggeringTask = "" OrElse Not task.Children(True).Contains(w.sTriggeringTask) Then
                                Select Case ctrl.eControl
                                    Case EventOrWalkControl.ControlEnum.Resume
                                        w.Resume()
                                    Case EventOrWalkControl.ControlEnum.Start
                                        w.Start()
                                    Case EventOrWalkControl.ControlEnum.Stop
                                        w.Stop()
                                    Case EventOrWalkControl.ControlEnum.Suspend
                                        w.Pause()
                                End Select
                                w.sTriggeringTask = task.Key
                            End If
                        End If
                    Next
                Next
            Next
            For Each e As clsEvent In Adventure.htblEvents.Values
                For Each ctrl As EventOrWalkControl In e.EventControls
                    If ctrl.eCompleteOrNot = EventOrWalkControl.CompleteOrNotEnum.Completion AndAlso ctrl.sTaskKey = task.Key Then
                        ' If a child task of the current task has affected the walk control, ignore this as a trigger
                        If e.sTriggeringTask = "" OrElse Not task.Children(True).Contains(e.sTriggeringTask) Then
                            Select Case ctrl.eControl
                                Case EventOrWalkControl.ControlEnum.Resume
                                    e.Resume()
                                Case EventOrWalkControl.ControlEnum.Start
                                    e.Start()
                                Case EventOrWalkControl.ControlEnum.Stop
                                    e.Stop()
                                Case EventOrWalkControl.ControlEnum.Suspend
                                    e.Pause()
                            End Select
                            e.sTriggeringTask = task.Key
                        End If
                    End If
                Next
            Next
        End If

        If Not bCalledFromEvent Then
            If task.ContinueToExecuteLowerPriority Then
                bContinue = True
                DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Continuing trying to execute lower priority tasks (multiple matches)")
            Else
                If bPass Then
                    If bTaskHasOutputNew Then
                        DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Task passes and has output.  Will not execute lower priority tasks")
                    Else
                        DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Task passes but has no output, therefore will continue to execute lower priority tasks")
                        bContinue = True
                    End If
                Else
                    If Adventure.TaskExecution = clsAdventure.TaskExecutionEnum.HighestPriorityPassingTask Then
                        If bTaskHasOutputNew Then
                            DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Task fails but has output.  Will continue trying execute lower priority tasks")
                            ' Hmm, we actually want to execute lower priority tasks EXCEPT library ones...
                            bContinue = True
                        Else
                            DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Task does not pass and also has no output, therefore will continue to execute lower priority tasks")
                            bContinue = True
                        End If
                    Else
                        If Not bTaskHasOutputNew Then
                            DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Task does not pass and also has no output, therefore will continue to execute lower priority tasks")
                            bContinue = True
                        End If
                    End If
                End If
            End If

            If bContinue AndAlso Not bChildTask Then
                UpdateStatusBar()
                EvaluateInput(task.Priority + 1, Not bPass AndAlso bTaskHasOutputNew)
            End If
        End If

        If htblResponsesPassTemp IsNot Nothing Then htblResponsesPass = htblResponsesPassTemp.Clone
        If htblResponsesFailTemp IsNot Nothing Then htblResponsesFail = htblResponsesFailTemp.Clone

        Return bPass
    End Function


    ' Returns True if the SubTask was completed successfully
    Public Function AttemptToExecuteSubTask(ByVal sTaskKey As String, ByVal sReferences() As String, ByVal bCalledFromEvent As Boolean, ByVal bChildTask As Boolean, ByVal sReferenceCommands() As String, ByRef bTaskHasOutputNew As Boolean, ByVal bPassingOnly As Boolean) As Boolean

        Try
            AttemptToExecuteSubTask = False

            Dim task As clsTask = Adventure.htblTasks(sTaskKey)

            ReDim NewReferences(sReferences.Length - 1)

            If sReferences.Length = 0 Then
                DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.Medium, "Checking reference free task " & task.Description)
            Else
                Dim sSubTask As String = ParentTaskCommand(task)
                For iRef As Integer = 0 To task.References.Count - 1
                    sSubTask = sSubTask.Replace(task.References(iRef), Adventure.GetNameFromKey(sReferences(iRef), False, False))
                    ' Set References to be ones from this particular subtask
                    Dim r As clsNewReference = Nothing
                    Select Case task.References(iRef)
                        Case "%object%", "%object1%", "%object2%", "%object3%", "%object4%", "%object5%", "%objects%"
                            r = New clsNewReference(ReferencesType.Object)
                        Case "%character%", "%character1%", "%character2%", "%character3%", "%character4%", "%character5%", "%characters%"
                            r = New clsNewReference(ReferencesType.Character)
                        Case "%location%", "%location1%", "%location2", "%location3", "%location4", "%location5"
                            r = New clsNewReference(ReferencesType.Location)
                        Case "%item%", "%item1%", "%item2%", "%item3%", "%item4%", "%item5%"
                            r = New clsNewReference(ReferencesType.Item)
                        Case "%direction%", "%direction1%", "%direction2%", "%direction3%", "%direction4%", "%direction5%"
                            r = New clsNewReference(ReferencesType.Direction)
                        Case "%text%", "%text1%", "%text2%", "%text3%", "%text4%", "%text5%"
                            r = New clsNewReference(ReferencesType.Text)
                        Case "%number%", "%number1%", "%number2%", "%number3%", "%number4%", "%number5%"
                            r = New clsNewReference(ReferencesType.Number)
                        Case Else
                            ErrMsg("To do")
                    End Select
                    r.sParentTask = sTaskKey
                    If sReferences(iRef) <> "" Then
                        Dim itm As New clsSingleItem
                        itm.MatchingPossibilities.Add(sReferences(iRef))
                        itm.sCommandReference = sReferenceCommands(iRef)
                        r.Items.Add(itm)
                        r.ReferenceMatch = task.References(iRef).Replace("%", "")
                    End If
                    If task.arlCommands.Count > 0 Then
                        Dim sRefs As StringArrayList = task.RefsInCommand(task.arlCommands(0))
                        Select Case task.References(iRef)
                            Case "%objects%", "%object1%", "%object2%", "%object3%", "%object4%", "%object5%", "%characters%", "%character1%", "%character2%", "%character3%", "%character4%", "%character5%", "%direction1%", "%direction2%", "%direction3%", "%direction4%", "%direction5%", "%number1%", "%number2%", "%number3%", "%number4%", "%number5%", "%text1%", "%text2%", "%text3%", "%text4%", "%text5%", "%item1%", "%item2%", "%item3%", "%item4%", "%item5%"
                                For iRef2 As Integer = 0 To sRefs.Count - 1
                                    If task.References(iRef) = sRefs(iRef2) Then
                                        r.Index = iRef2
                                        Exit For
                                    End If
                                Next
                        End Select
                    End If
                    NewReferences(iRef) = r
                Next
                DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.Medium, "Checking " & CStr(IIf(sReferences.Length = 1, "single", IIf(sReferences.Length = 2, "double", "triple or more"))) & " reference task " & sSubTask)
            End If

            Dim sMessage As String = ""
            Dim bPass As Boolean = PassRestrictions(task.arlRestrictions, , task)
            Dim bOutputMessages As Boolean = False

            If bPass Then
                DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.Medium, "Passed Restrictions")

                ' Remove any failing refs if this ref passes
                For Each sFailMessage As String In htblResponsesFail.OrderedKeys
                    Dim refsFail() As clsNewReference = CType(htblResponsesFail(sFailMessage), clsNewReference())

                    For iRef As Integer = 0 To refsFail.Length - 1
                        Dim refFail As clsNewReference = refsFail(iRef)
                        If refFail IsNot Nothing Then
                            For iItm As Integer = 0 To refFail.Items.Count - 1
                                Dim itmFail As clsSingleItem = refFail.Items(iItm)
                                ' There should only be one matching possibility here, so no need to iterate them
                                If NewReferences(iRef) Is Nothing OrElse NewReferences(iRef).Items(iItm).MatchingPossibilities(0) <> itmFail.MatchingPossibilities(0) Then
                                    ' This fail is different from this pass                            
                                    GoTo NextMessage
                                End If
                            Next iItm
                        End If
                    Next iRef

                    ' Ok, lets remove the failed one
                    htblResponsesFail.Remove(sFailMessage)
                    Exit For ' There should only be one matching the refs, so bomb out so we don't cause problem iterating loop
NextMessage:
                Next sFailMessage

                ' If this task contains references, see if we have a specific task
                If task.Children.Count > 0 Then DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Checking whether any of our child tasks should override...")

                Dim bShouldParentOutputText As Boolean = True
                Dim bShouldParentExecuteTasks As Boolean = True
                Dim bAnyChildHasOutput As Boolean = False
                Dim lAfterChildren As New List(Of String)


                For Each sChildTask As String In task.Children ' in order or priority...
                    Dim tasChild As clsTask = Adventure.htblTasks(sChildTask)
                    If task.TaskType = clsTask.TaskTypeEnum.Specific Then
                        ' If our parent is a Specific task, we may need to pad our Specifics out with any that have been dropped
                        If task.Specifics.Length <> tasChild.Specifics.Length Then
                            ' TODO - These may be in the wrong order if we're matching on a secondary command!
                            Dim newSpecs(task.Specifics.Length - 1) As clsTask.Specific
                            Dim iChild As Integer = 0
                            For i As Integer = 0 To task.Specifics.Length - 1
                                newSpecs(i) = task.Specifics(i)
                                If newSpecs(i).Keys.Count = 0 OrElse (newSpecs(i).Keys.Count = 1 AndAlso newSpecs(i).Keys(0) = "") Then
                                    newSpecs(i) = tasChild.Specifics(iChild)
                                    iChild += 1
                                End If
                            Next
                            tasChild.Specifics = newSpecs
                        End If
                    End If
                    Dim iMatchCount As Integer = 0

                    If RefsMatchSpecifics(tasChild) Then ' This should remove the ref so it doesn't get processed when we execute the main task
                        DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.Medium, "Overriding child task found: " & tasChild.Description)

                        Select Case tasChild.SpecificOverrideType
                            Case clsTask.SpecificOverrideTypeEnum.BeforeActionsOnly, clsTask.SpecificOverrideTypeEnum.BeforeTextAndActions, clsTask.SpecificOverrideTypeEnum.BeforeTextOnly, clsTask.SpecificOverrideTypeEnum.Override
                                iDebugIndent += 1
                                Dim bContinueExecutingTasks As Boolean = False

                                If tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.Override Then
                                    DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.High, "Override Parent")
                                Else
                                    DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.High, "Run Before Parent")
                                End If

                                ' Make a note of how many failing responses we have, so we know if this task has failing output
                                Dim iFailRefsBefore As Integer = 0
                                If task.References.Count = 0 Then
                                    iFailRefsBefore += htblResponsesFail.Count
                                Else
                                    For Each responses() As clsNewReference In htblResponsesFail.Values
                                        For Each response As clsNewReference In responses
                                            iFailRefsBefore += response.Items.Count
                                        Next
                                    Next
                                End If


                                Dim bChildTaskHasOutput As Boolean = False
                                If AttemptToExecuteTask(tasChild.Key, bCalledFromEvent, , True, bContinueExecutingTasks, bChildTaskHasOutput, , bAnyChildHasOutput) Then
                                    DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.High, "Child task passes")
                                    If tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeActionsOnly OrElse tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeTextAndActions Then
                                        DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.Medium, "Execute Parent actions...")
                                    Else
                                        bShouldParentExecuteTasks = False
                                    End If
                                    If tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeTextAndActions OrElse tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeTextOnly Then
                                        DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.Medium, "Output Parent text...")
                                    Else
                                        bShouldParentOutputText = False
                                    End If
                                    AttemptToExecuteSubTask = True
                                Else
                                    DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.High, "Child task fails")
                                    ' Ok, compare failing output vs what it was before - if we have failing output, this takes precedence over parent if set
                                    Dim iFailRefsAfter As Integer = 0
                                    If task.References.Count = 0 Then
                                        iFailRefsAfter += htblResponsesFail.Count
                                    Else
                                        For Each responses() As clsNewReference In htblResponsesFail.Values
                                            For Each response As clsNewReference In responses
                                                iFailRefsAfter += response.Items.Count
                                            Next
                                        Next
                                    End If

                                    If iFailRefsAfter > iFailRefsBefore Then
                                        If tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeTextAndActions OrElse tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeTextOnly OrElse tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.Override Then
                                            bShouldParentOutputText = False
                                        End If
                                        If tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeTextAndActions OrElse tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeActionsOnly OrElse tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.Override Then
                                            bShouldParentExecuteTasks = False
                                        End If
                                        bAnyChildHasOutput = True
                                    End If
                                End If


                                bTaskHasOutputNew = bTaskHasOutputNew OrElse bChildTaskHasOutput ' If the child task has output, ensure we are aware of it
                                iDebugIndent -= 1

                                If Not bContinueExecutingTasks Then
                                    DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.Medium, "Do not continue executing other child tasks.")
                                    Exit For
                                Else
                                    DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.Medium, "Continue executing other child tasks.")
                                End If

                            Case Else
                                lAfterChildren.Add(tasChild.Key)
                                If tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.AfterActionsOnly OrElse tasChild.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.AfterTextOnly Then
                                    ' TODO - Need to check PassRestrictions to see if we need to suppress parent Text or Actions
                                End If
                        End Select
                    End If
                Next

                Dim sBeforeActionsMessage As String = ""
                Dim iResponsePosition As Integer = -1
                If task.eDisplayCompletion = clsTask.BeforeAfterEnum.Before AndAlso bShouldParentOutputText Then
                    ' We may have already printed these refs out in a child task, so only print them here if we haven't done that
                    If NewReferences.Length > 0 Then PrintOutReferences()
                    sMessage = task.CompletionMessage.ToString
                    If sMessage Is Nothing Then sMessage = ""
                    DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, sMessage)
                    ' Replace any functions that could be affected by our actions...                    
                    ' Can't do this, because %TheObject[%objects%]% will interpret so we get multiple entries if do get all 
                    ' But then it means that %ParentOf[%objects%]% will be incorrect if action moves it
                    'sMessage = sMessage.Replace("%ParentOf[", "%PrevParentOf[").Replace("%ListObjectsOn[", "%PrevListObjectsOn[").Replace(".Parent.", ".PrevParent.")

                    ' If we do this then drop all gives us: Ok, I drop X.  Ok, I drop Y.  Ok, I drop Z.
                    UserSession.bDisplaying = True
                    bTestingOutput = True ' Ensure any DisplayOnce descriptions aren't marked as displayed, as we'll mark them in final output (Display)
                    sBeforeActionsMessage = ReplaceExpressions(ReplaceFunctions(sMessage))
                    bTestingOutput = False
                    If sBeforeActionsMessage = sMessage Then sBeforeActionsMessage = ""
                    UserSession.bDisplaying = False
                    If sBeforeActionsMessage = "" Then
                        ' It is safe to add the response now
                        If Not task.AggregateOutput Then sMessage = ReplaceExpressions(ReplaceFunctions(sMessage))
                        If AddResponse(bOutputMessages, sMessage, sReferences, True) Then bTaskHasOutputNew = True
                    Else
                        ' The response changes with functions, so we can't add yet until we know whether the actions affect the output
                        Dim htblResponses As OrderedHashTable = CType(IIf(bPass, htblResponsesPass, htblResponsesFail), OrderedHashTable)
                        iResponsePosition = htblResponses.Count
                    End If
                End If

                task.Completed = True
                If bShouldParentExecuteTasks Then ExecuteActions(task, bCalledFromEvent, bTaskHasOutputNew)

                If sBeforeActionsMessage <> "" Then
                    ' Check to see if the actions had any effect on the message.  If so, add the replaced message.  If not, add the unreplaced message
                    UserSession.bDisplaying = True
                    bTestingOutput = True
                    If sBeforeActionsMessage <> ReplaceExpressions(ReplaceFunctions(sMessage)) Then sMessage = sBeforeActionsMessage
                    bTestingOutput = False
                    UserSession.bDisplaying = False
                    If Not task.AggregateOutput Then sMessage = ReplaceExpressions(ReplaceFunctions(sMessage))
                    If AddResponse(bOutputMessages, sMessage, sReferences, True, iResponsePosition) Then bTaskHasOutputNew = True
                End If

                If task.eDisplayCompletion = clsTask.BeforeAfterEnum.After AndAlso bShouldParentOutputText Then
                    sMessage = task.CompletionMessage.ToString
                    If sMessage Is Nothing Then sMessage = ""
                    If Not task.AggregateOutput Then sMessage = ReplaceExpressions(ReplaceFunctions(sMessage))
                    DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, sMessage)
                    If AddResponse(bOutputMessages, sMessage, sReferences, True) Then bTaskHasOutputNew = True
                End If


                For Each sChildTask As String In lAfterChildren
                    iDebugIndent += 1

                    Dim tasChild As clsTask = Adventure.htblTasks(sChildTask)
                    DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.High, "Run After Parent")

                    Dim bChildTaskHasOutput As Boolean = False
                    Dim bContinueExecutingTasks As Boolean = False
                    If AttemptToExecuteTask(tasChild.Key, bCalledFromEvent, , True, bContinueExecutingTasks, bChildTaskHasOutput) Then
                        DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.High, "Child task passes")
                        AttemptToExecuteSubTask = True
                    Else
                        DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.High, "Child task fails")
                    End If


                    bTaskHasOutputNew = bTaskHasOutputNew OrElse bChildTaskHasOutput ' If the child task has output, ensure we are aware of it
                    iDebugIndent -= 1
                    If Not bContinueExecutingTasks Then
                        DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.Medium, "Do not continue executing other child tasks.")
                        Exit For
                    Else
                        DebugPrint(ItemEnum.Task, tasChild.Key, DebugDetailLevelEnum.Medium, "Continue executing other child tasks.")
                    End If
                Next

                AttemptToExecuteSubTask = True
            Else
                If Not bPassingOnly Then
                    DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.Medium, "Failed Restrictions")
                    sMessage = sRestrictionText

                    If sMessage Is Nothing Then sMessage = ""
                    DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, sMessage)
                    If NewReferences.Length > 0 Then PrintOutReferences()

                    If task.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.AfterTextAndActions OrElse task.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.AfterTextOnly Then
                        ' Although this is a failing restriction, it is being output _after_ a successful task, so we need to append to the success list :-/
                        If sMessage <> "" Then bPass = True
                    End If
                End If
            End If

            If AddResponse(bOutputMessages, sMessage, sReferences, bPass) Then bTaskHasOutputNew = True

        Catch ex As Exception
            ErrMsg("Error executing subtask " & sTaskKey, ex)
        End Try
    End Function

    Private Class clsReponse
        Public sReferences() As String
        Public bPass As Boolean
    End Class

    ' Determines whether tags in a message correspond to actual output
    Private Function bHasOutput(ByVal sText As String) As Boolean
        If sText = "" Then
            Return False
        Else
            If StripCarats(sText) = "" Then
                Dim sTextLower As String = sText.ToLower
                For Each sValid As String In New String() {"<br>", "<center>", "<centre>", "<i>", "</i>", "<b>", "</b>", "<u>", "</u>", "<c>", "</c>", "<font", "</font>", "<right>", "</right>", "<left>", "</left>", "<del>", "<wait", "<cls>", "<img "}
                    If sTextLower.Contains(sValid) Then Return True
                Next
                ' Ok, so we have an unknown tag.  But the user may be text replacing it, so check for that
                If Adventure.htblALRs.ContainsKey(sText) Then
                    Return True
                Else
                    Return False
                End If
            Else
                Return True
            End If
        End If
    End Function


    ' Returns True if a response was added, or False otherwise
    Private Function AddResponse(ByRef bOutputMessages As Boolean, ByVal sMessage As String, ByVal sReferences() As String, ByVal bPass As Boolean, Optional iPosition As Integer = -1) As Boolean

        If bOutputMessages OrElse Not bHasOutput(sMessage) Then Return False

        Dim htblResponses As OrderedHashTable = CType(IIf(bPass, htblResponsesPass, htblResponsesFail), OrderedHashTable)

        If htblResponses.ContainsKey(sMessage) Then
            ' Add our new references to the ones already there
            For iRef As Integer = 0 To sReferences.Length - 1
                If CType(htblResponses(sMessage), clsNewReference()).Length > iRef AndAlso CType(htblResponses(sMessage), clsNewReference())(iRef) IsNot Nothing AndAlso Not CType(htblResponses(sMessage), clsNewReference())(iRef).ContainsKey(sReferences(iRef)) Then
                    CType(htblResponses(sMessage), clsNewReference())(iRef).Items.Add(New clsSingleItem(sReferences(iRef)))
                End If
            Next
        Else
            ' Store our references
            If iPosition > -1 Then
                htblResponses.Insert(sMessage, NewReferences, iPosition)
            Else
                htblResponses.Add(sMessage, NewReferences)
            End If
        End If

        bOutputMessages = True
        Return True

    End Function

    Public Sub UncompleteTask(ByVal sTaskKey As String)

        Adventure.htblTasks(sTaskKey).Completed = False
        For Each c As clsCharacter In Adventure.htblCharacters.Values
            For Each w As clsWalk In c.arlWalks
                For Each wc As EventOrWalkControl In w.WalkControls
                    If wc.sTaskKey = sTaskKey AndAlso wc.eCompleteOrNot = EventOrWalkControl.CompleteOrNotEnum.UnCompletion Then
                        Select Case wc.eControl
                            Case EventOrWalkControl.ControlEnum.Start
                                If w.Status = clsEvent.StatusEnum.NotYetStarted Then w.Start()
                            Case EventOrWalkControl.ControlEnum.Stop
                                If w.Status = clsEvent.StatusEnum.Running Then w.Stop()
                            Case EventOrWalkControl.ControlEnum.Suspend
                                If w.Status = clsEvent.StatusEnum.Running Then w.Pause()
                            Case EventOrWalkControl.ControlEnum.Resume
                                If w.Status = clsEvent.StatusEnum.Paused Then w.Resume()
                        End Select
                    End If
                Next
            Next
        Next
        For Each e As clsEvent In Adventure.htblEvents.Values
            For Each ec As EventOrWalkControl In e.EventControls
                If ec.sTaskKey = sTaskKey AndAlso ec.eCompleteOrNot = EventOrWalkControl.CompleteOrNotEnum.UnCompletion Then
                    Select Case ec.eControl
                        Case EventOrWalkControl.ControlEnum.Start
                            If e.Status = clsEvent.StatusEnum.NotYetStarted Then e.Start()
                        Case EventOrWalkControl.ControlEnum.Stop
                            If e.Status = clsEvent.StatusEnum.Running Then e.Stop()
                        Case EventOrWalkControl.ControlEnum.Suspend
                            If e.Status = clsEvent.StatusEnum.Running Then e.Pause()
                        Case EventOrWalkControl.ControlEnum.Resume
                            If e.Status = clsEvent.StatusEnum.Paused Then e.Resume()
                    End Select
                End If
            Next
        Next

    End Sub


    ' Return the token that the reference we're looking at is 
    Private Function GetReferenceKey(ByVal sTaskCommand As String, ByVal iRefNum As Integer) As String

        Dim sTokens() As String = sTaskCommand.Split(" "c)
        Dim iWhichRef As Integer = 0
        For Each sToken As String In sTokens
            Select Case sToken
                Case "%character1%", "%character2%", "%character3%", "%character4%", "%character5%", "%characters%", "%direction%", "%number%", "%numbers%", "%object1%", "%object2%", "%object3%", "%object4%", "%object5%", "%objects%", "%text%"
                    iWhichRef += 1
            End Select
            If iWhichRef = iRefNum Then
                Return sToken.Replace("%object", "ReferencedObject").Replace("%direction", "ReferencedDirection").Replace("%", "")
                Exit For
            End If
        Next

        Return ""

    End Function

    Private Sub ExecuteSingleAction(ByVal actx As clsAction, ByVal sTaskCommand As String, ByVal task As clsTask, Optional ByVal bCalledFromEvent As Boolean = False, Optional ByRef bTaskHasOutputNew As Boolean = False)
        Try
            Dim act As clsAction = actx.Copy

            ' Replace references
            Select Case act.sKey1
                Case "ReferencedObject", "ReferencedObject1", "ReferencedObject2", "ReferencedObject3", "ReferencedObject4", "ReferencedObject5"
                    act.sKey1 = GetReference(act.sKey1)
                Case "ReferencedObjects"
                    For iRef As Integer = 0 To NewReferences.Length - 1
                        With NewReferences(iRef)
                            If .Items.Count > 0 AndAlso .ReferenceType = ReferencesType.Object AndAlso GetReferenceKey(sTaskCommand, iRef + 1) = "ReferencedObjects" Then
                                For iOb As Integer = 0 To .Items.Count - 1
                                    If .Items(iOb).MatchingPossibilities.Count > 0 Then
                                        act.sKey1 = .Items(iOb).MatchingPossibilities(0)
                                        ExecuteSingleAction(act, sTaskCommand, task, bTaskHasOutputNew)
                                    End If
                                Next
                            End If
                        End With
                    Next
                    Exit Sub
                Case "ReferencedDirection", "ReferencedDirection1", "ReferencedDirection2", "ReferencedDirection3", "ReferencedDirection4", "ReferencedDirection5"
                    act.sKey1 = GetReference(act.sKey1)
                Case "ReferencedCharacter", "ReferencedCharacter1", "ReferencedCharacter2", "ReferencedCharacter3", "ReferencedCharacter4", "ReferencedCharacter5"
                    act.sKey1 = GetReference(act.sKey1)
                Case "ReferencedLocation", "ReferencedLocation1", "ReferencedLocation2", "ReferencedLocation3", "ReferencedLocation4", "ReferencedLocation5"
                    act.sKey1 = GetReference(act.sKey1)
                Case "ReferencedItem", "ReferencedItem1", "ReferencedItem2", "ReferencedItem3", "ReferencedItem4", "ReferencedItem5"
                    act.sKey1 = GetReference(act.sKey1)
                Case "ReferencedNumber", "ReferencedNumber1", "ReferencedNumber2", "ReferencedNumber3", "ReferencedNumber4", "ReferencedNumber5"
                    act.sKey1 = GetReference(act.sKey1)
                Case "%Player%"
                    act.sKey1 = Adventure.Player.Key

            End Select
            Select Case act.sKey2
                Case "ReferencedObject", "ReferencedObject1", "ReferencedObject2", "ReferencedObject3", "ReferencedObject4", "ReferencedObject5"
                    act.sKey2 = GetReference(act.sKey2)
                Case "ReferencedObjects"
                    For iRef As Integer = 0 To NewReferences.Length - 1
                        With NewReferences(iRef)
                            If .Items.Count > 0 AndAlso .ReferenceType = ReferencesType.Object AndAlso GetReferenceKey(sTaskCommand, iRef) = "ReferencedObjects" Then
                                For iOb As Integer = 0 To .Items.Count - 1
                                    If .Items(iOb).MatchingPossibilities.Count > 0 Then
                                        act.sKey2 = .Items(iOb).MatchingPossibilities(0)
                                        ExecuteSingleAction(act, sTaskCommand, task, bTaskHasOutputNew)
                                    End If
                                Next
                            End If
                        End With
                    Next
                    Exit Sub
                Case "ReferencedDirection", "ReferencedDirection1", "ReferencedDirection2", "ReferencedDirection3", "ReferencedDirection4", "ReferencedDirection5"
                    act.sKey2 = GetReference(act.sKey2)
                Case "ReferencedCharacter", "ReferencedCharacter1", "ReferencedCharacter2", "ReferencedCharacter3", "ReferencedCharacter4", "ReferencedCharacter5"
                    act.sKey2 = GetReference(act.sKey2)
                Case "ReferencedLocation", "ReferencedLocation1", "ReferencedLocation2", "ReferencedLocation3", "ReferencedLocation4", "ReferencedLocation5"
                    act.sKey2 = GetReference(act.sKey2)
                Case "ReferencedItem", "ReferencedItem1", "ReferencedItem2", "ReferencedItem3", "ReferencedItem4", "ReferencedItem5"
                    act.sKey2 = GetReference(act.sKey2)
                Case "ReferencedNumber", "ReferencedNumber1", "ReferencedNumber2", "ReferencedNumber3", "ReferencedNumber4", "ReferencedNumber5"
                    act.sKey2 = GetReference(act.sKey2)
                Case "%Player%"
                    act.sKey2 = Adventure.Player.Key

            End Select

            For i As Integer = 0 To 5
                Dim iRef As Integer = i
                If iRef > 0 Then iRef -= 1

                Dim sRefText As String = "%text" & If(i > 0, i.ToString, "").ToString & "%"
                If act.eItem = clsAction.ItemEnum.Conversation AndAlso act.StringValue = sRefText Then
                    act.StringValue = Adventure.sReferencedText(iRef)
                End If
            Next


            Dim bBadKeys As Boolean = False
            Select Case act.eItem
                Case clsAction.ItemEnum.EndGame, clsAction.ItemEnum.Time
                Case clsAction.ItemEnum.SetVariable, clsAction.ItemEnum.IncreaseVariable, clsAction.ItemEnum.DecreaseVariable, clsAction.ItemEnum.SetTasks, clsAction.ItemEnum.Conversation
                    If act.sKey1 Is Nothing Then bBadKeys = True
                Case Else
                    If act.sKey1 Is Nothing OrElse act.sKey2 Is Nothing Then bBadKeys = True
            End Select
            If bBadKeys Then
                DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.High, "Bad key(s) for action ")
                Exit Sub
            End If

            Select Case act.eItem
                Case clsAction.ItemEnum.MoveObject, clsAction.ItemEnum.AddObjectToGroup, clsAction.ItemEnum.RemoveObjectFromGroup
                    Dim obs As ObjectHashTable = Nothing

                    Select Case act.eMoveObjectWhat
                        Case clsAction.MoveObjectWhatEnum.Object
                            obs = New ObjectHashTable
                            obs.Add(Adventure.htblObjects(act.sKey1), act.sKey1)
                        Case clsAction.MoveObjectWhatEnum.EverythingAtLocation
                            obs = New ObjectHashTable
                            For Each ob As clsObject In Adventure.htblObjects.Values
                                If Not ob.IsStatic AndAlso ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InLocation AndAlso ob.Location.Key = act.sKey1 Then
                                    obs.Add(ob, ob.Key)
                                End If
                            Next
                        Case clsAction.MoveObjectWhatEnum.EverythingHeldBy
                            obs = Adventure.htblCharacters(act.sKey1).HeldObjects
                        Case clsAction.MoveObjectWhatEnum.EverythingInGroup
                            obs = New ObjectHashTable
                            For Each sKey As String In Adventure.htblGroups(act.sKey1).arlMembers
                                obs.Add(Adventure.htblObjects(sKey), sKey)
                            Next
                        Case clsAction.MoveObjectWhatEnum.EverythingInside
                            obs = Adventure.htblObjects(act.sKey1).Children(clsObject.WhereChildrenEnum.InsideObject)
                        Case clsAction.MoveObjectWhatEnum.EverythingOn
                            obs = Adventure.htblObjects(act.sKey1).Children(clsObject.WhereChildrenEnum.OnObject)
                        Case clsAction.MoveObjectWhatEnum.EverythingWithProperty
                            obs = New ObjectHashTable
                            Dim prop As clsProperty = Adventure.htblObjectProperties(act.sKey1)
                            For Each ob As clsObject In Adventure.htblObjects.Values
                                If ob.HasProperty(prop.Key) Then
                                    If prop.Type = clsProperty.PropertyTypeEnum.SelectionOnly Then
                                        obs.Add(ob, ob.Key)
                                    Else
                                        If ob.GetPropertyValue(prop.Key) = act.sPropertyValue Then obs.Add(ob, ob.Key)
                                    End If
                                End If
                            Next
                        Case clsAction.MoveObjectWhatEnum.EverythingWornBy
                            obs = Adventure.htblCharacters(act.sKey1).WornObjects
                    End Select

                    If obs IsNot Nothing Then
                        For Each ob As clsObject In obs.Values

                            Select Case act.eItem
                                Case clsAction.ItemEnum.MoveObject
                                    Dim dest As New clsObjectLocation
                                    Select Case act.eMoveObjectTo
                                        Case clsAction.MoveObjectToEnum.InsideObject
                                            dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject
                                            dest.Key = act.sKey2
                                        Case clsAction.MoveObjectToEnum.OntoObject
                                            dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject
                                            dest.Key = act.sKey2
                                        Case clsAction.MoveObjectToEnum.ToCarriedBy
                                            dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter
                                            dest.Key = act.sKey2

                                        Case clsAction.MoveObjectToEnum.ToLocation
                                            If ob.IsStatic Then
                                                If act.sKey2 = "Hidden" Then ' i.e. Nowhere
                                                    dest.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.NoRooms
                                                Else
                                                    dest.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                                                    dest.Key = act.sKey2
                                                End If
                                            Else
                                                If act.sKey2 = "Hidden" Then
                                                    dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.Hidden
                                                Else
                                                    dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InLocation
                                                    dest.Key = act.sKey2
                                                End If
                                            End If

                                        Case clsAction.MoveObjectToEnum.ToPartOfCharacter
                                            dest.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter
                                            dest.Key = act.sKey2

                                        Case clsAction.MoveObjectToEnum.ToPartOfObject
                                            dest.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.PartOfObject
                                            dest.Key = act.sKey2

                                        Case clsAction.MoveObjectToEnum.ToLocationGroup
                                            If ob.IsStatic Then
                                                dest.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.LocationGroup
                                                dest.Key = act.sKey2
                                            Else
                                                ' Need to select one room at random                            
                                                Dim group As clsGroup = Adventure.htblGroups(act.sKey2)
                                                dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InLocation
                                                dest.Key = group.RandomKey
                                            End If

                                        Case clsAction.MoveObjectToEnum.ToSameLocationAs
                                            If ob.IsStatic Then
                                                If Adventure.htblCharacters.ContainsKey(act.sKey2) Then
                                                    ' Move Static ob to same location as a character
                                                    Dim chDest As clsCharacter = Adventure.htblCharacters(act.sKey2)
                                                    If chDest.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation Then
                                                        dest.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                                                        dest.Key = chDest.Location.Key
                                                    ElseIf chDest.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.Hidden Then
                                                        dest.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.NoRooms
                                                    End If
                                                ElseIf Adventure.htblObjects.ContainsKey(act.sKey2) Then
                                                    ' Move Static ob to same location as an object
                                                    Dim obDest As clsObject = Adventure.htblObjects(act.sKey2)
                                                    If obDest.IsStatic Then
                                                        dest = obDest.Location.Copy
                                                    Else
                                                        If obDest.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.Hidden Then
                                                            dest.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.NoRooms
                                                        Else
                                                            dest.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                                                            For Each loc As clsLocation In obDest.LocationRoots.Values
                                                                dest.Key = loc.Key
                                                                Exit For
                                                            Next
                                                        End If
                                                    End If
                                                Else
                                                    ErrMsg("Cannot move object to same location as " & act.sKey2 & " - key not found!")
                                                End If

                                            Else
                                                If Adventure.htblCharacters.ContainsKey(act.sKey2) Then
                                                    ' Move dynamic ob to same location as a character
                                                    Dim chDest As clsCharacter = Adventure.htblCharacters(act.sKey2)

                                                    Select Case chDest.Location.ExistWhere
                                                        Case clsCharacterLocation.ExistsWhereEnum.AtLocation
                                                            dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InLocation
                                                            dest.Key = chDest.Location.Key
                                                        Case clsCharacterLocation.ExistsWhereEnum.Hidden
                                                            dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.Hidden
                                                        Case clsCharacterLocation.ExistsWhereEnum.InObject
                                                            ' Move the object inside the object we're in - but only if it allows dynamic objects
                                                            Dim obDest As clsObject = Adventure.htblObjects(chDest.Location.Key)
                                                            If obDest.IsContainer Then
                                                                dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject
                                                                dest.Key = chDest.Location.Key
                                                            Else
                                                                dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InLocation
                                                                dest.Key = chDest.Location.LocationKey
                                                            End If
                                                        Case clsCharacterLocation.ExistsWhereEnum.OnCharacter
                                                            ' Move to the location that the character is at
                                                            dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InLocation
                                                            dest.Key = chDest.Location.LocationKey
                                                        Case clsCharacterLocation.ExistsWhereEnum.OnObject
                                                            ' Move the object onto the object we're on - but only if it allows dynamic objects
                                                            Dim obDest As clsObject = Adventure.htblObjects(chDest.Location.Key)
                                                            If obDest.HasSurface Then
                                                                dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject
                                                                dest.Key = chDest.Location.Key
                                                            Else
                                                                dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InLocation
                                                                dest.Key = chDest.Location.LocationKey
                                                            End If
                                                    End Select

                                                ElseIf Adventure.htblObjects.ContainsKey(act.sKey2) Then
                                                    Dim obDest As clsObject = Adventure.htblObjects(act.sKey2)

                                                    If obDest.IsStatic Then
                                                        ' If the static object exists in more than one place, pick a random location 
                                                        Select Case obDest.Location.StaticExistWhere
                                                            Case clsObjectLocation.StaticExistsWhereEnum.AllRooms
                                                                TODO("Move dynamic object to one of all rooms")
                                                            Case clsObjectLocation.StaticExistsWhereEnum.LocationGroup
                                                                TODO("Move dynamic object to one of a location group")
                                                            Case clsObjectLocation.StaticExistsWhereEnum.NoRooms
                                                                obDest.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.Hidden
                                                            Case clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter
                                                                TODO("Move dynamic object to same room as character")
                                                            Case clsObjectLocation.StaticExistsWhereEnum.PartOfObject
                                                                TODO("Move dynamic object to same room as part of object")
                                                            Case clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                                                                dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InLocation
                                                                dest.Key = obDest.Location.Key
                                                        End Select
                                                    Else
                                                        If obDest.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.Hidden Then
                                                            dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.Hidden
                                                        Else
                                                            dest = obDest.Location.Copy
                                                        End If
                                                    End If
                                                Else
                                                    ErrMsg("Cannot move object to same location as " & act.sKey2 & " - key not found!")
                                                End If

                                            End If
                                        Case clsAction.MoveObjectToEnum.ToWornBy
                                            dest.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                                            dest.Key = act.sKey2
                                    End Select
                                    ob.Move(dest)

                                Case clsAction.ItemEnum.AddObjectToGroup
                                    If Not Adventure.htblGroups(act.sKey2).arlMembers.Contains(ob.Key) Then Adventure.htblGroups(act.sKey2).arlMembers.Add(ob.Key)
                                    ob.ResetInherited()
                                Case clsAction.ItemEnum.RemoveObjectFromGroup
                                    If Adventure.htblGroups(act.sKey2).arlMembers.Contains(ob.Key) Then Adventure.htblGroups(act.sKey2).arlMembers.Remove(ob.Key)
                                    ob.ResetInherited()
                            End Select
                        Next
                    End If

                Case clsAction.ItemEnum.MoveCharacter, clsAction.ItemEnum.AddCharacterToGroup, clsAction.ItemEnum.RemoveCharacterFromGroup
                    Dim chars As CharacterHashTable = Nothing

                    Select Case act.eMoveCharacterWho
                        Case clsAction.MoveCharacterWhoEnum.Character
                            chars = New CharacterHashTable
                            chars.Add(Adventure.htblCharacters(act.sKey1), act.sKey1)
                        Case clsAction.MoveCharacterWhoEnum.EveryoneAtLocation
                            chars = Adventure.htblLocations(act.sKey1).CharactersDirectlyInLocation(True)
                        Case clsAction.MoveCharacterWhoEnum.EveryoneInGroup
                            chars = New CharacterHashTable
                            For Each sKey As String In Adventure.htblGroups(act.sKey1).arlMembers
                                chars.Add(Adventure.htblCharacters(sKey), sKey)
                            Next
                        Case clsAction.MoveCharacterWhoEnum.EveryoneInside
                            chars = Adventure.htblObjects(act.sKey1).ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject)
                        Case clsAction.MoveCharacterWhoEnum.EveryoneOn
                            chars = Adventure.htblObjects(act.sKey1).ChildrenCharacters(clsObject.WhereChildrenEnum.OnObject)
                        Case clsAction.MoveCharacterWhoEnum.EveryoneWithProperty
                            chars = New CharacterHashTable
                            Dim prop As clsProperty = Adventure.htblCharacterProperties(act.sKey1)
                            For Each ch As clsCharacter In Adventure.htblCharacters.Values
                                If ch.HasProperty(prop.Key) Then
                                    If prop.Type = clsProperty.PropertyTypeEnum.SelectionOnly Then
                                        chars.Add(ch, ch.Key)
                                    Else
                                        If ch.GetPropertyValue(prop.Key) = act.sPropertyValue Then chars.Add(ch, ch.Key)
                                    End If
                                End If
                            Next
                    End Select

                    If chars IsNot Nothing Then
                        For Each ch As clsCharacter In chars.Values

                            Select Case act.eItem
                                Case clsAction.ItemEnum.MoveCharacter
                                    Dim dest As New clsCharacterLocation(ch)

                                    ' Default new destination to current location
                                    dest.ExistWhere = ch.Location.ExistWhere
                                    dest.Key = ch.Location.Key
                                    dest.Position = ch.Location.Position

                                    Select Case act.eMoveCharacterTo
                                        Case clsAction.MoveCharacterToEnum.InDirection
                                            Dim d As DirectionsEnum = CType([Enum].Parse(GetType(DirectionsEnum), act.sKey2), DirectionsEnum)
                                            Dim dDetails As clsDirection = Nothing
                                            If Adventure.htblLocations.ContainsKey(ch.Location.LocationKey) Then dDetails = Adventure.htblLocations(ch.Location.LocationKey).arlDirections(d)
                                            Dim sRouteErrorTask As String = sRouteError ' because sRouteError gets overwritten by checking route restrictions
                                            Dim sRestrictionTextTemp As String = sRestrictionText
                                            sRestrictionText = ""

                                            If dDetails IsNot Nothing AndAlso ch.HasRouteInDirection(d, False) Then
                                                If Adventure.htblLocations.ContainsKey(dDetails.LocationKey) Then
                                                    dest.Key = dDetails.LocationKey
                                                ElseIf Adventure.htblGroups.ContainsKey(dDetails.LocationKey) Then
                                                    dest.Key = Adventure.htblGroups(dDetails.LocationKey).RandomKey
                                                End If
                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                            Else
                                                If sRestrictionText <> "" Then
                                                    Display(sRestrictionText)
                                                Else
                                                    ' Need to grab out the restriction text from the movement task
                                                    If sRouteErrorTask <> "" Then Display(sRouteErrorTask)
                                                End If
                                                dest = Nothing
                                            End If
                                            sRestrictionText = sRestrictionText

                                        Case clsAction.MoveCharacterToEnum.ToLocation
                                            If act.sKey2 = "Hidden" Then
                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.Hidden
                                                dest.Key = ""
                                            Else
                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                                dest.Key = act.sKey2
                                            End If
                                        Case clsAction.MoveCharacterToEnum.ToLocationGroup
                                            dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                            dest.Key = Adventure.htblGroups(act.sKey2).RandomKey
                                        Case clsAction.MoveCharacterToEnum.ToLyingOn
                                            dest.Position = clsCharacterLocation.PositionEnum.Lying
                                            If act.sKey2 = THEFLOOR Then
                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                                dest.Key = ch.Location.LocationKey
                                            Else
                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject
                                                dest.Key = act.sKey2
                                            End If
                                        Case clsAction.MoveCharacterToEnum.ToSameLocationAs
                                            If Adventure.htblCharacters.ContainsKey(act.sKey2) Then
                                                dest.ExistWhere = Adventure.htblCharacters(act.sKey2).Location.ExistWhere
                                                dest.Key = Adventure.htblCharacters(act.sKey2).Location.Key
                                            ElseIf Adventure.htblObjects.ContainsKey(act.sKey2) Then
                                                With Adventure.htblObjects(act.sKey2)
                                                    If .IsStatic Then
                                                        Select Case .Location.StaticExistWhere
                                                            Case clsObjectLocation.StaticExistsWhereEnum.AllRooms, clsObjectLocation.StaticExistsWhereEnum.LocationGroup
                                                                ' Doesn't make sense to map
                                                            Case clsObjectLocation.StaticExistsWhereEnum.NoRooms
                                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.Hidden
                                                            Case clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter
                                                                TODO("Move Char to same location as object that is part of a character")
                                                            Case clsObjectLocation.StaticExistsWhereEnum.PartOfObject
                                                                TODO("Move Char to same location as object that is part of an object")
                                                            Case clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                                                dest.Key = .Location.Key
                                                        End Select
                                                    Else
                                                        Select Case .Location.DynamicExistWhere
                                                            Case clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter, clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                                                                dest.ExistWhere = Adventure.htblCharacters(.Key).Location.ExistWhere
                                                                dest.Key = Adventure.htblCharacters(.Key).Location.Key
                                                            Case clsObjectLocation.DynamicExistsWhereEnum.Hidden
                                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.Hidden
                                                            Case clsObjectLocation.DynamicExistsWhereEnum.InLocation
                                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                                                dest.Key = .Location.Key
                                                            Case clsObjectLocation.DynamicExistsWhereEnum.InObject, clsObjectLocation.DynamicExistsWhereEnum.OnObject
                                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                                                For Each l As clsLocation In Adventure.htblObjects(.Key).LocationRoots.Values
                                                                    dest.Key = l.Key
                                                                    Exit For
                                                                Next
                                                        End Select
                                                    End If

                                                End With
                                            End If
                                        Case clsAction.MoveCharacterToEnum.ToSittingOn
                                            dest.Position = clsCharacterLocation.PositionEnum.Sitting
                                            If act.sKey2 = THEFLOOR Then
                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                                dest.Key = ch.Location.LocationKey
                                            Else
                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject
                                                dest.Key = act.sKey2
                                            End If
                                        Case clsAction.MoveCharacterToEnum.ToStandingOn
                                            dest.Position = clsCharacterLocation.PositionEnum.Standing
                                            If act.sKey2 = THEFLOOR Then
                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                                dest.Key = ch.Location.LocationKey
                                            Else
                                                dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject
                                                dest.Key = act.sKey2
                                            End If
                                        Case clsAction.MoveCharacterToEnum.ToSwitchWith
                                            If ch.Key = Adventure.Player.Key OrElse act.sKey2 = Adventure.Player.Key Then
                                                ' Don't move the characters, but change which one is the player
                                                Dim eCurrentPerspective As PerspectiveEnum = Adventure.Player.Perspective
                                                Dim sOldPlayerKey As String = Adventure.Player.Key
                                                Adventure.Player.CharacterType = clsCharacter.CharacterTypeEnum.NonPlayer
                                                If ch.Key = Adventure.Player.Key Then
                                                    Adventure.Player = Adventure.htblCharacters(act.sKey2)
                                                Else
                                                    Adventure.Player = Adventure.htblCharacters(ch.Key)
                                                End If
                                                Adventure.Player.CharacterType = clsCharacter.CharacterTypeEnum.Player
                                                Adventure.Player.Perspective = eCurrentPerspective
                                                ' If the old Player character has any descriptors that match any pronouns for the player perspective, move them to the new Player
                                                ' TODO - Change this to configurable pronouns
                                                With Adventure.htblCharacters(sOldPlayerKey)
                                                    For iDescriptor As Integer = .arlDescriptors.Count - 1 To 0 Step -1
                                                        Dim sDescriptor As String = .arlDescriptors(iDescriptor)
                                                        Dim sPronouns As String() = {}
                                                        Select Case eCurrentPerspective
                                                            Case PerspectiveEnum.FirstPerson
                                                                sPronouns = New String() {"I", "me", "myself"}
                                                            Case PerspectiveEnum.SecondPerson
                                                                sPronouns = New String() {"I", "me", "myself", "you", "yourself"} ' include 1st in 2nd
                                                            Case PerspectiveEnum.ThirdPerson

                                                        End Select
                                                        For Each sPronoun As String In sPronouns
                                                            If sPronoun.ToLower = sDescriptor.ToLower Then
                                                                .arlDescriptors.RemoveAt(iDescriptor)
                                                                If Not Adventure.Player.arlDescriptors.Contains(sPronoun) Then Adventure.Player.arlDescriptors.Add(sPronoun)
                                                            End If
                                                        Next
                                                    Next
                                                End With
                                            Else
                                                ' Move the characters about
                                                Dim loc As clsCharacterLocation = ch.Location
                                                ch.Location = Adventure.htblCharacters(act.sKey2).Location
                                                Adventure.htblCharacters(act.sKey2).Location = loc
                                            End If
                                        Case clsAction.MoveCharacterToEnum.InsideObject
                                            dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.InObject
                                            dest.Key = act.sKey2
                                        Case clsAction.MoveCharacterToEnum.OntoCharacter
                                            If Adventure.htblCharacters.ContainsKey(act.sKey2) Then
                                                If ch.Key = act.sKey2 OrElse ch.Children(True).ContainsKey(act.sKey2) Then
                                                    DisplayError("Recursive character relationship")
                                                Else
                                                    dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnCharacter
                                                    dest.Key = act.sKey2
                                                End If
                                            End If
                                        Case clsAction.MoveCharacterToEnum.ToParentLocation
                                            dest.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                            Dim sCurrent As String = ch.Location.Key
                                            If Adventure.htblObjects.ContainsKey(sCurrent) Then
                                                For Each sKey As String In Adventure.htblObjects(sCurrent).LocationRoots.Keys
                                                    dest.Key = sKey
                                                    Exit For
                                                Next
                                            ElseIf Adventure.htblCharacters.ContainsKey(sCurrent) Then
                                                dest.Key = Adventure.htblCharacters(sCurrent).Location.LocationKey
                                            End If
                                        Case Else
                                            TODO("Move Character to " & act.eMoveCharacterTo.ToString)
                                    End Select
                                    If dest IsNot Nothing Then ch.Move(dest)

                                Case clsAction.ItemEnum.AddCharacterToGroup
                                    If Not Adventure.htblGroups(act.sKey2).arlMembers.Contains(ch.Key) Then Adventure.htblGroups(act.sKey2).arlMembers.Add(ch.Key)
                                    ch.ResetInherited()
                                Case clsAction.ItemEnum.RemoveCharacterFromGroup
                                    If Adventure.htblGroups(act.sKey2).arlMembers.Contains(ch.Key) Then Adventure.htblGroups(act.sKey2).arlMembers.Remove(ch.Key)
                                    ch.ResetInherited()
                            End Select

                        Next
                    End If


                Case clsAction.ItemEnum.AddLocationToGroup, clsAction.ItemEnum.RemoveLocationFromGroup
                    Dim locs As LocationHashTable = Nothing

                    Select Case act.eMoveLocationWhat
                        Case clsAction.MoveLocationWhatEnum.Location
                            locs = New LocationHashTable
                            locs.Add(Adventure.htblLocations(act.sKey1), act.sKey1)
                        Case clsAction.MoveLocationWhatEnum.LocationOf
                            If Adventure.htblCharacters.ContainsKey(act.sKey1) Then
                                locs = New LocationHashTable
                                Dim sLocKey As String = Adventure.htblCharacters(act.sKey1).Location.LocationKey
                                If sLocKey <> HIDDEN Then locs.Add(Adventure.htblLocations(sLocKey), act.sKey1)
                            ElseIf Adventure.htblObjects.ContainsKey(act.sKey1) Then
                                locs = Adventure.htblObjects(act.sKey1).LocationRoots
                            End If
                        Case clsAction.MoveLocationWhatEnum.EverywhereInGroup
                            locs = New LocationHashTable
                            For Each sKey As String In Adventure.htblGroups(act.sKey1).arlMembers
                                locs.Add(Adventure.htblLocations(sKey), sKey)
                            Next
                        Case clsAction.MoveLocationWhatEnum.EverywhereWithProperty
                            locs = New LocationHashTable
                            Dim prop As clsProperty = Adventure.htblLocationProperties(act.sKey1)
                            For Each loc As clsLocation In Adventure.htblLocations.Values
                                If loc.HasProperty(prop.Key) Then
                                    If prop.Type = clsProperty.PropertyTypeEnum.SelectionOnly Then
                                        locs.Add(loc, loc.Key)
                                    Else
                                        If loc.GetPropertyValue(prop.Key) = act.sPropertyValue Then locs.Add(loc, loc.Key)
                                    End If
                                End If
                            Next
                    End Select

                    If locs IsNot Nothing Then
                        For Each loc As clsLocation In locs.Values

                            Select Case act.eItem
                                Case clsAction.ItemEnum.AddLocationToGroup
                                    If Not Adventure.htblGroups(act.sKey2).arlMembers.Contains(loc.Key) Then Adventure.htblGroups(act.sKey2).arlMembers.Add(loc.Key)
                                    loc.ResetInherited()
                                Case clsAction.ItemEnum.RemoveLocationFromGroup
                                    If Adventure.htblGroups(act.sKey2).arlMembers.Contains(loc.Key) Then Adventure.htblGroups(act.sKey2).arlMembers.Remove(loc.Key)
                                    loc.ResetInherited()
                            End Select

                        Next
                    End If


                Case clsAction.ItemEnum.SetProperties
                    If Adventure.htblObjects.ContainsKey(act.sKey1) Then
                        Dim ob As clsObject = Adventure.htblObjects(act.sKey1)

                        Select Case act.sKey2
                            Case OBJECTARTICLE
                                Dim sResult As String = EvaluateExpression(act.sPropertyValue)
                                If sResult Is Nothing AndAlso act.sPropertyValue <> "" Then sResult = act.sPropertyValue
                                ob.Article = sResult
                            Case OBJECTPREFIX
                                Dim sResult As String = EvaluateExpression(act.sPropertyValue)
                                If sResult Is Nothing AndAlso act.sPropertyValue <> "" Then sResult = act.sPropertyValue
                                ob.Prefix = sResult
                            Case OBJECTNOUN
                                Dim sResult As String = EvaluateExpression(act.sPropertyValue)
                                If sResult Is Nothing AndAlso act.sPropertyValue <> "" Then sResult = act.sPropertyValue
                                ob.arlNames(0) = sResult
                            Case Else
                                Dim prop As clsProperty = Nothing
                                If ob.HasProperty(act.sKey2) Then prop = ob.GetProperty(act.sKey2)

                                If prop Is Nothing Then
                                    ' We're trying to add this as a property
                                    If Adventure.htblObjectProperties.ContainsKey(act.sKey2) Then
                                        prop = CType(Adventure.htblObjectProperties(act.sKey2).Clone, clsProperty)
                                        prop.Selected = True
                                        ob.AddProperty(prop)

                                        Select Case prop.Type
                                            Case clsProperty.PropertyTypeEnum.SelectionOnly
                                                ' Nothing more to do        
                                            Case clsProperty.PropertyTypeEnum.ObjectKey
                                                Dim sObKey As String = act.sPropertyValue
                                                If sObKey.StartsWith("Referenced") Then sObKey = GetReference(sObKey)
                                                ob.SetPropertyValue(prop.Key, sObKey)
                                        End Select
                                    Else
                                        DebugPrint(ItemEnum.Task, act.sKey1, DebugDetailLevelEnum.Error, "Property " & act.sKey2 & " not found.")
                                    End If
                                Else
                                    If act.StringValue = sUNSELECTED Then
                                        ' We're trying to remove this property
                                        ob.RemoveProperty(act.sKey2)
                                    Else
                                        Select Case prop.Type
                                            Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.Text, clsProperty.PropertyTypeEnum.StateList, clsProperty.PropertyTypeEnum.ValueList
                                                ' Could be dropdown or expression
                                                Dim sResult As String = EvaluateExpression(act.sPropertyValue)
                                                If sResult Is Nothing AndAlso act.sPropertyValue <> "" Then sResult = act.sPropertyValue
                                                prop.Value = sResult
                                            Case clsProperty.PropertyTypeEnum.ObjectKey, clsProperty.PropertyTypeEnum.CharacterKey, clsProperty.PropertyTypeEnum.LocationKey
                                                Dim sObKey As String = act.sPropertyValue
                                                If sObKey.StartsWith("Referenced") Then sObKey = GetReference(sObKey)
                                                prop.Value = sObKey
                                            Case Else
                                                prop.Value = act.sPropertyValue
                                        End Select
                                    End If
                                End If
                        End Select

                    ElseIf Adventure.htblCharacters.ContainsKey(act.sKey1) Then
                        Dim prop As clsProperty = Nothing
                        Dim ch As clsCharacter = Adventure.htblCharacters(act.sKey1)
                        If ch.HasProperty(act.sKey2) Then prop = ch.GetProperty(act.sKey2)

                        If prop Is Nothing Then
                            Select Case act.sKey2
                                Case CHARACTERPROPERNAME
                                    prop = New clsProperty
                                    prop.Key = act.sKey2
                                    prop.Type = clsProperty.PropertyTypeEnum.Text
                            End Select
                        End If

                        If prop Is Nothing Then
                            If act.StringValue = sSELECTED Then
                                ' We're trying to add this as a property
                                If Adventure.htblCharacterProperties.ContainsKey(act.sKey2) Then
                                    prop = CType(Adventure.htblCharacterProperties(act.sKey2).Clone, clsProperty)
                                    If prop.Type = clsProperty.PropertyTypeEnum.SelectionOnly Then
                                        prop.Selected = True
                                        ch.AddProperty(prop)
                                    End If
                                Else
                                    DebugPrint(ItemEnum.Task, act.sKey1, DebugDetailLevelEnum.Error, "Property " & act.sKey2 & " not found.")
                                End If
                            Else
                                DebugPrint(ItemEnum.Task, act.sKey1, DebugDetailLevelEnum.Error, "Property " & act.sKey2 & " not found.")
                            End If
                        Else
                            If act.StringValue = sUNSELECTED Then
                                ' We're trying to remove this property
                                ch.RemoveProperty(act.sKey2)
                            Else
                                Select Case prop.Type
                                    Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.Text, clsProperty.PropertyTypeEnum.StateList, clsProperty.PropertyTypeEnum.ValueList
                                        ' Could be dropdown or expression
                                        Dim sResult As String = EvaluateExpression(act.sPropertyValue)
                                        If sResult Is Nothing AndAlso act.sPropertyValue <> "" Then sResult = act.sPropertyValue
                                        prop.Value = sResult
                                    Case clsProperty.PropertyTypeEnum.ObjectKey, clsProperty.PropertyTypeEnum.CharacterKey, clsProperty.PropertyTypeEnum.LocationKey
                                        Dim sObKey As String = act.sPropertyValue
                                        If sObKey.StartsWith("Referenced") Then sObKey = GetReference(sObKey)
                                        prop.Value = sObKey
                                    Case Else
                                        prop.Value = act.sPropertyValue
                                End Select
                            End If
                        End If

                        Select Case act.sKey2
                            Case CHARACTERPROPERNAME
                                ch.ProperName = prop.Value
                            Case "CharacterPosition"
                                ch.Location.ResetPosition()
                        End Select

                    ElseIf Adventure.htblLocations.ContainsKey(act.sKey1) Then
                        Dim prop As clsProperty = Nothing
                        If Adventure.htblLocations(act.sKey1).HasProperty(act.sKey2) Then prop = Adventure.htblLocations(act.sKey1).GetProperty(act.sKey2)

                        If prop Is Nothing Then
                            If act.StringValue = sSELECTED Then
                                ' We're trying to add this as a property
                                If Adventure.htblLocationProperties.ContainsKey(act.sKey2) Then
                                    prop = CType(Adventure.htblLocationProperties(act.sKey2).Clone, clsProperty)
                                    If prop.Type = clsProperty.PropertyTypeEnum.SelectionOnly Then
                                        prop.Selected = True
                                        Adventure.htblLocations(act.sKey1).AddProperty(prop)
                                    End If
                                Else
                                    DebugPrint(ItemEnum.Task, act.sKey1, DebugDetailLevelEnum.Error, "Property " & act.sKey2 & " not found.")
                                End If
                            Else
                                DebugPrint(ItemEnum.Task, act.sKey1, DebugDetailLevelEnum.Error, "Property " & act.sKey2 & " not found.")
                            End If
                        Else
                            If act.StringValue = sUNSELECTED Then
                                ' We're trying to remove this property
                                Adventure.htblLocations(act.sKey1).RemoveProperty(act.sKey2)
                            Else
                                Select Case prop.Type
                                    Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.Text, clsProperty.PropertyTypeEnum.StateList, clsProperty.PropertyTypeEnum.ValueList
                                        ' Could be dropdown or expression
                                        Dim sResult As String = EvaluateExpression(act.sPropertyValue)
                                        If sResult Is Nothing AndAlso act.sPropertyValue <> "" Then sResult = act.sPropertyValue
                                        prop.Value = sResult
                                    Case clsProperty.PropertyTypeEnum.ObjectKey, clsProperty.PropertyTypeEnum.CharacterKey, clsProperty.PropertyTypeEnum.LocationKey
                                        Dim sObKey As String = act.sPropertyValue
                                        If sObKey.StartsWith("Referenced") Then sObKey = GetReference(sObKey)
                                        prop.Value = sObKey
                                    Case Else
                                        prop.Value = act.sPropertyValue
                                End Select
                                If prop.Key = "CharacterPosition" Then Adventure.htblCharacters(act.sKey1).Location.ResetPosition()
                            End If
                        End If
                    End If

                Case clsAction.ItemEnum.SetVariable, clsAction.ItemEnum.IncreaseVariable, clsAction.ItemEnum.DecreaseVariable
                    Dim var As clsVariable = Adventure.htblVariables(act.sKey1)
                    Dim sExpr As String = act.StringValue
                    If act.eItem = clsAction.ItemEnum.IncreaseVariable Then sExpr = "%" & var.Name & "% + " & sExpr
                    If act.eItem = clsAction.ItemEnum.DecreaseVariable Then sExpr = "%" & var.Name & "% - " & sExpr

                    If act.eVariables = clsAction.VariablesEnum.Assignment Then
                        Dim iIndex As Integer = 1
                        If var.Length > 1 AndAlso Not act.sKey2 Is Nothing Then
                            If act.sKey2.StartsWith("ReferencedNumber") Then
                                iIndex = SafeInt(GetReference(act.sKey2))
                            ElseIf IsNumeric(act.sKey2) Then
                                iIndex = CInt(Val(act.sKey2))
                            Else
                                iIndex = Adventure.htblVariables(act.sKey2).IntValue
                            End If
                        End If
                        If var.Key <> "Score" OrElse (task IsNot Nothing AndAlso Not task.Scored) Then
                            var.SetToExpression(sExpr, iIndex)
                            If var.Key = "Score" Then
                                task.Scored = True
                                Adventure.Score = var.IntValue
                            End If
                        End If
                    Else
                        For iLoop As Integer = act.IntValue To CInt(act.sKey2)
                            var.SetToExpression(sExpr, iLoop)
                        Next
                    End If

                Case clsAction.ItemEnum.SetTasks
                    Dim tas2X As clsTask = Adventure.htblTasks(act.sKey1)

                    Dim iFrom As Integer = 1
                    Dim iTo As Integer = 1

                    If act.sPropertyValue <> "" Then
                        iFrom = act.IntValue
                        iTo = SafeInt(act.sPropertyValue)
                    End If

                    For iLoop As Integer = iFrom To iTo
                        If act.eSetTasks = clsAction.SetTasksEnum.Execute Then
                            ' Store the existing refs
                            Dim oExistingRefs() As clsNewReference = NewReferences

                            DebugPrint(ItemEnum.Task, act.sKey1, DebugDetailLevelEnum.High, "Executing task '" & tas2X.Description & "'.")
                            If act.StringValue <> "" Then
                                ' Rewrite the references based on our parameters
                                Dim sParams() As String = act.StringValue.Split("|"c)
                                Dim iWhichNewRefAreWeLookingAt As Integer = -1
                                Dim ReferencesNew(sParams.Length - 1) As clsNewReference
                                For Each sParam As String In sParams
                                    iWhichNewRefAreWeLookingAt += 1

                                    ' Find each ref in the new task that our parameter corresponds to
                                    Dim bFoundMatchingRef As Boolean = False
                                    Dim iWhichOldRefAreWeLookingAt As Integer = -1
                                    For Each sRef As String In tas2X.References
                                        iWhichOldRefAreWeLookingAt += 1
                                        ' Again, we may be looking outside NewRefs if we're looking at subtask with different refs... :-/
                                        If sParam = sRef AndAlso iWhichOldRefAreWeLookingAt < NewReferences.Length Then ' Ok, found same ref, so we just pass the ref thru
                                            ReferencesNew(iWhichNewRefAreWeLookingAt) = NewReferences(iWhichOldRefAreWeLookingAt)
                                            bFoundMatchingRef = True
                                        End If
                                    Next
                                    If Not bFoundMatchingRef Then
                                        ' Need to work this out on our own...
                                        Dim UserDefinedRef As clsNewReference = Nothing

                                        If sParam.Contains(".") Then
                                            ' Hmm, let's see if this is an OO function
                                            For Each sProp As String In Adventure.htblAllProperties.Keys
                                                If sParam.EndsWith("." & sProp) Then
                                                    Dim prop As clsProperty = Adventure.htblAllProperties(sProp)
                                                    Select Case prop.Type
                                                        Case clsProperty.PropertyTypeEnum.CharacterKey
                                                            UserDefinedRef = New clsNewReference(ReferencesType.Character)
                                                        Case clsProperty.PropertyTypeEnum.Integer
                                                            UserDefinedRef = New clsNewReference(ReferencesType.Number)
                                                        Case clsProperty.PropertyTypeEnum.LocationGroupKey
                                                            UserDefinedRef = New clsNewReference(ReferencesType.Text)
                                                        Case clsProperty.PropertyTypeEnum.LocationKey
                                                            UserDefinedRef = New clsNewReference(ReferencesType.Location)
                                                        Case clsProperty.PropertyTypeEnum.ObjectKey
                                                            UserDefinedRef = New clsNewReference(ReferencesType.Object)
                                                        Case clsProperty.PropertyTypeEnum.SelectionOnly
                                                            Select Case prop.PropertyOf
                                                                Case clsProperty.PropertyOfEnum.AnyItem
                                                                    UserDefinedRef = New clsNewReference(ReferencesType.Item)
                                                                Case clsProperty.PropertyOfEnum.Characters
                                                                    UserDefinedRef = New clsNewReference(ReferencesType.Character)
                                                                Case clsProperty.PropertyOfEnum.Locations
                                                                    UserDefinedRef = New clsNewReference(ReferencesType.Location)
                                                                Case clsProperty.PropertyOfEnum.Objects
                                                                    UserDefinedRef = New clsNewReference(ReferencesType.Object)
                                                            End Select
                                                        Case clsProperty.PropertyTypeEnum.StateList
                                                            UserDefinedRef = New clsNewReference(ReferencesType.Text)
                                                        Case clsProperty.PropertyTypeEnum.Text
                                                            UserDefinedRef = New clsNewReference(ReferencesType.Text)
                                                        Case clsProperty.PropertyTypeEnum.ValueList
                                                            UserDefinedRef = New clsNewReference(ReferencesType.Number)
                                                    End Select
                                                    Exit For
                                                End If
                                            Next
                                            If UserDefinedRef Is Nothing Then
                                                If sParam.EndsWith(".Worn") Then
                                                    UserDefinedRef = New clsNewReference(ReferencesType.Object)
                                                ElseIf sParam.EndsWith(".List") Then
                                                    TODO()
                                                ElseIf sParam.EndsWith(".Count") Then
                                                    UserDefinedRef = New clsNewReference(ReferencesType.Number)
                                                ElseIf sParam.EndsWith(".Exits") Then
                                                    UserDefinedRef = New clsNewReference(ReferencesType.Direction)
                                                End If
                                            End If
                                        End If
                                        If UserDefinedRef Is Nothing Then
                                            ' Gotta guess the type of ref...
                                            Select Case sLeft(sParam, 6).ToLower
                                                Case "%convc"
                                                    UserDefinedRef = New clsNewReference(ReferencesType.Character)
                                                Case "%paren"
                                                    UserDefinedRef = New clsNewReference(ReferencesType.Object)
                                                Case "%text%"
                                                    UserDefinedRef = New clsNewReference(ReferencesType.Text)
                                                Case "%loop%", "%numbe"
                                                    UserDefinedRef = New clsNewReference(ReferencesType.Number)
                                                Case Else
                                                    If IsNumeric(sLeft(sParam, 6).ToLower) Then
                                                        UserDefinedRef = New clsNewReference(ReferencesType.Number)
                                                    Else
                                                        UserDefinedRef = New clsNewReference(ReferencesType.Object)
                                                    End If
                                            End Select
                                        End If
                                        UserDefinedRef.sParentTask = tas2X.Key

                                        ' Now work out, e.g. %ParentOf[%objects%]% ...
                                        Dim sFunctionRef As String = ReplaceFunctions(sParam)
                                        If sFunctionRef.ToLower = "%loop%" Then sFunctionRef = iLoop.ToString

                                        If Not sFunctionRef.Contains("***") Then
                                            Dim listRefs As New List(Of String)
                                            If sFunctionRef.Contains("|") Then
                                                For Each sRef As String In sFunctionRef.Split("|"c)
                                                    listRefs.Add(sRef)
                                                Next
                                            Else
                                                listRefs.Add(sFunctionRef)
                                            End If

                                            For Each sRef As String In listRefs
                                                Dim itm As New clsSingleItem
                                                If sRef = "nothing" Then sRef = Nothing ' List function 
                                                itm.MatchingPossibilities.Add(sRef)
                                                UserDefinedRef.Items.Add(itm)
                                            Next
                                            If sParam.StartsWith("%ParentOf") Then
                                                UserDefinedRef.ReferenceType = ReferencesType.Object
                                            End If
                                        Else
                                            DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.High, "Error calculating parameter " & sParam)
                                        End If
                                        ReferencesNew(iWhichNewRefAreWeLookingAt) = UserDefinedRef
                                    End If
                                Next
                                NewReferences = ReferencesNew
                                PrintOutReferences()
                            Else
                                ' Explicitly clear the refs for this task being called
                                NewReferences = Nothing
                            End If
                            ' was True in ChildTask.  But it's not a child, it's a seperate task call... 
                            ' was bCalledFromEvent in second param - but think this should be set if calling from task
                            ' Re the above, a reason why would be good.  Means later we don't do the continue? check
                            Dim bSubTaskHasOutput As Boolean = False
                            ' Did have EvaluateResponses = True, but we need to evaluate at the end in case we are inserting responses before ones in actions
                            AttemptToExecuteTask(act.sKey1, bCalledFromEvent, , True, , bSubTaskHasOutput, False) 'True)
                            If bSubTaskHasOutput Then bTaskHasOutputNew = True
                            NewReferences = oExistingRefs
                        Else
                            If tas2X.Completed Then
                                DebugPrint(ItemEnum.Task, act.sKey1, DebugDetailLevelEnum.High, "Task '" & tas2X.Description & "' being uncompleted.")

                                If tas2X.Completed Then
                                    ' Check any walks/events to see if anything triggers on this task completing
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        For Each w As clsWalk In c.arlWalks
                                            For Each ctrl As EventOrWalkControl In w.WalkControls
                                                If ctrl.eCompleteOrNot = EventOrWalkControl.CompleteOrNotEnum.UnCompletion AndAlso ctrl.sTaskKey = tas2X.Key Then
                                                    Select Case ctrl.eControl
                                                        Case EventOrWalkControl.ControlEnum.Resume
                                                            If w.Status = clsWalk.StatusEnum.Paused Then w.Resume()
                                                        Case EventOrWalkControl.ControlEnum.Start
                                                            If w.Status <> clsWalk.StatusEnum.Running Then w.Start()
                                                        Case EventOrWalkControl.ControlEnum.Stop
                                                            If w.Status = clsWalk.StatusEnum.Running Then w.Stop()
                                                        Case EventOrWalkControl.ControlEnum.Suspend
                                                            If w.Status = clsWalk.StatusEnum.Running Then w.Pause()
                                                    End Select
                                                End If
                                            Next
                                        Next
                                    Next
                                    For Each e As clsEvent In Adventure.htblEvents.Values
                                        For Each ctrl As EventOrWalkControl In e.EventControls
                                            If ctrl.eCompleteOrNot = EventOrWalkControl.CompleteOrNotEnum.UnCompletion AndAlso ctrl.sTaskKey = tas2X.Key Then
                                                Select Case ctrl.eControl
                                                    Case EventOrWalkControl.ControlEnum.Resume
                                                        If e.Status = clsWalk.StatusEnum.Paused Then e.Resume()
                                                    Case EventOrWalkControl.ControlEnum.Start
                                                        If e.Status <> clsWalk.StatusEnum.Running Then e.Start()
                                                    Case EventOrWalkControl.ControlEnum.Stop
                                                        If e.Status = clsWalk.StatusEnum.Running Then e.Stop()
                                                    Case EventOrWalkControl.ControlEnum.Suspend
                                                        If e.Status = clsWalk.StatusEnum.Running Then e.Pause()
                                                End Select
                                            End If
                                        Next
                                    Next
                                End If

                                tas2X.Completed = False
                            End If
                        End If
                    Next

                Case clsAction.ItemEnum.Time
                    ' This isn't perfect - not sure what'll happen with any fail messages
                    For Each sMessage As String In htblResponsesPass.OrderedKeys
                        Dim refs As clsNewReference() = CType(htblResponsesPass(sMessage), clsNewReference())

                        NewReferences = refs
                        Display(sMessage)
                    Next
                    htblResponsesPass.Clear()
                    For i As Integer = 0 To EvaluateExpression(act.StringValue, True) - 1
                        TurnBasedStuff()
                    Next
                    htblResponsesPass.Clear()

                Case clsAction.ItemEnum.EndGame
                    Adventure.eGameState = act.eEndgame

                Case clsAction.ItemEnum.Conversation
                    Select Case act.eConversation
                        Case clsAction.ConversationEnum.EnterConversation
                            Adventure.sConversationCharKey = act.sKey1
                        Case clsAction.ConversationEnum.LeaveConversation
                            If Adventure.sConversationCharKey = act.sKey1 Then Adventure.sConversationCharKey = ""
                        Case Else
                            ExecuteConversation(act.sKey1, act.eConversation, act.StringValue, bTaskHasOutputNew)
                            'bTaskHasOutput = True
                    End Select

            End Select

        Catch ex As Exception
            ErrMsg("Error executing action " & actx.Summary, ex)
        End Try

    End Sub

    Private Sub ExecuteConversation(ByVal sCharKey As String, ByVal ConvType As clsAction.ConversationEnum, ByVal sCommandOrSubject As String, ByRef bTaskHasOutputNew As Boolean)

        DebugPrint(ItemEnum.Character, sCharKey, DebugDetailLevelEnum.Medium, "Execute Conversation " & ConvType.ToString & ": " & sCommandOrSubject)

        ' If currently in a conversation with a different character, search for an Implicit Farewell for other char
        If Adventure.sConversationCharKey <> "" AndAlso Adventure.sConversationCharKey <> sCharKey Then
            Dim farewell As clsTopic = FindConversationNode(Adventure.htblCharacters(Adventure.sConversationCharKey), ConvType, "")
            If farewell IsNot Nothing Then
                If AddResponse(False, farewell.oConversation.ToString, New String() {}, True) Then bTaskHasOutputNew = True
            End If

            Adventure.sConversationCharKey = ""
            Adventure.sConversationNode = ""
        End If

        Dim ConvChar As clsCharacter = Adventure.htblCharacters(sCharKey)

        ' If not currently in conversation and ConvType != Intro, then search for an Implicit Intro for that char
        If Adventure.sConversationCharKey = "" Then ' AndAlso ConvType <> clsAction.ConversationEnum.Greet Then     
            ' Try to find an explicit intro
            Dim intro As clsTopic = FindConversationNode(ConvChar, ConvType Or clsAction.ConversationEnum.Greet, sCommandOrSubject)
            ' If not, look for an implicit one
            If intro Is Nothing Then intro = FindConversationNode(ConvChar, clsAction.ConversationEnum.Greet, "")
            If intro IsNot Nothing Then
                If AddResponse(False, intro.oConversation.ToString, New String() {}, True) Then bTaskHasOutputNew = True
                Adventure.sConversationNode = intro.Key
                If intro.bAsk OrElse intro.bTell OrElse intro.bCommand Then ' We matched an explicit intro, so no need to look further
                    Adventure.sConversationCharKey = sCharKey
                    If intro.arlActions.Count > 0 Then ExecuteActions(intro.arlActions, bTaskHasOutputNew)
                    Exit Sub
                End If
                ' TODO - Run the implicit actions if we didn't find an explict match later.
            End If
        End If

        ' Enter conversation with character
        Adventure.sConversationCharKey = sCharKey

        ' Find conversation node (try to match on Farewell commands first)
        Dim topic As clsTopic = Nothing
        Dim sRestrictionTextTemp As String = sRestrictionText
        sRestrictionText = ""
        If ConvType = clsAction.ConversationEnum.Command Then topic = FindConversationNode(ConvChar, ConvType Or clsAction.ConversationEnum.Farewell, sCommandOrSubject)
        If topic Is Nothing Then
            topic = FindConversationNode(ConvChar, ConvType, sCommandOrSubject)
        Else
            Adventure.sConversationCharKey = ""
            Adventure.sConversationNode = ""
        End If

        If topic IsNot Nothing Then
            If AddResponse(False, topic.oConversation.ToString, New String() {}, True) Then bTaskHasOutputNew = True
            ' If topic has children, set the conversation node
            If ConvChar.htblTopics.DoesTopicHaveChildren(topic.Key) Then
                Adventure.sConversationNode = topic.Key
            Else
                If Not topic.bStayInNode Then Adventure.sConversationNode = ""
            End If
            If topic.arlActions.Count > 0 Then ExecuteActions(topic.arlActions, bTaskHasOutputNew)
        Else
            ' Hmm, no conversation found.  Need to give a default response back...
            Adventure.sConversationNode = ""
            Dim sMessage As String = ""
            If sRestrictionText <> "" Then
                sMessage = sRestrictionText
            Else
                ' TODO: Need to make this configurable within Generator
                Select Case ConvType
                    Case clsAction.ConversationEnum.Ask
                        If Adventure.dVersion < 5 Then
                            sMessage = "%CharacterName[" & ConvChar.Key & "]% does not respond to your question."
                        Else
                            sMessage = "%CharacterName[" & ConvChar.Key & "]% doesn't appear to understand you."
                        End If
                    Case clsAction.ConversationEnum.Farewell
                        sMessage = "%CharacterName[" & ConvChar.Key & "]% doesn't appear to understand you."
                    Case clsAction.ConversationEnum.Greet
                        sMessage = "%CharacterName[" & ConvChar.Key & "]% doesn't appear to understand you."
                    Case clsAction.ConversationEnum.Tell
                        sMessage = "%CharacterName[" & ConvChar.Key & "]% doesn't appear to understand you."
                    Case clsAction.ConversationEnum.Command
                        sMessage = "%CharacterName[" & ConvChar.Key & "]% ignores you."
                        'AddResponse(bOutputMessages, ConvChar.Name & " ignores you.",  sReferences, bPass)
                End Select
            End If

            If AddResponse(False, sMessage, New String() {}, True) Then bTaskHasOutputNew = True
        End If
        sRestrictionText = sRestrictionTextTemp ' Dunno if we really need to do this, but may as well to be safe
    End Sub


    Friend Sub TriggerTimerEvent(ByVal sEventKey As String, ByVal se As clsEvent.SubEvent)
        With Adventure.htblEvents(sEventKey)
            .RunSubEvent(se)
        End With
        Display(vbCrLf & vbCrLf, True)
        CheckEndOfGame()
    End Sub

    Friend Function FindConversationNode(ByVal ConvChar As clsCharacter, ByVal ConvType As clsAction.ConversationEnum, ByVal sCommandOrSubject As String) As clsTopic
        Dim iConvType As Integer = CInt(ConvType)
        Dim bFarewell As Boolean = False
        If iConvType >= CInt(clsAction.ConversationEnum.Farewell) Then
            bFarewell = True
            iConvType -= CInt(clsAction.ConversationEnum.Farewell)
        End If
        Dim bCommand As Boolean = False
        If iConvType >= CInt(clsAction.ConversationEnum.Command) Then
            bCommand = True
            iConvType -= CInt(clsAction.ConversationEnum.Command)
        End If
        Dim bTell As Boolean = False
        If iConvType >= CInt(clsAction.ConversationEnum.Tell) Then
            bTell = True
            iConvType -= CInt(clsAction.ConversationEnum.Tell)
        End If
        Dim bAsk As Boolean = False
        If iConvType >= CInt(clsAction.ConversationEnum.Ask) Then
            bAsk = True
            iConvType -= CInt(clsAction.ConversationEnum.Ask)
        End If
        Dim bIntro As Boolean = False
        If iConvType >= CInt(clsAction.ConversationEnum.Greet) Then
            bIntro = True
            iConvType -= CInt(clsAction.ConversationEnum.Greet)
        End If

        ' Iterate thru all the leaves of the current node
        With ConvChar
            ' Should we sort our topics, perhaps by length?
            Dim dfHighestPercent As Double = 0
            Dim iMostMatches As Integer = 0
            Dim topicBest As clsTopic = Nothing

            For Each topic As clsTopic In .htblTopics.Values
                Dim iMatchedKeywords As Integer = 0

                If (topic.ParentKey = "" OrElse topic.ParentKey = Adventure.sConversationNode) AndAlso (Not bIntro OrElse topic.bIntroduction) AndAlso (Not bFarewell OrElse topic.bFarewell) AndAlso (bCommand = topic.bCommand) AndAlso (Not bAsk OrElse topic.bAsk) AndAlso (Not bTell OrElse topic.bTell) Then

                    If bAsk OrElse bTell Then
                        ' Keyword matching
                        ' Find the node that matches the most keywords
                        ' Then if there are more than one, pick the one that matches most as a percentage
                        Dim sKeywords As String() = topic.Keywords.Split(","c)
                        Dim bLowPriority As Boolean = False

                        For Each sKeyword As String In sKeywords
                            If ContainsWord(sCommandOrSubject, sKeyword.ToLower.Trim) OrElse sKeyword = "*" Then
                                If PassRestrictions(topic.arlRestrictions) Then iMatchedKeywords += 1 ' Return topic
                                If sKeyword = "*" Then bLowPriority = True
                            End If
                        Next
                        Dim dfPercentMatched As Double = iMatchedKeywords / sKeywords.Length
                        If bLowPriority AndAlso dfPercentMatched = 1 Then dfPercentMatched = 0.001

                        If iMatchedKeywords > iMostMatches OrElse (iMatchedKeywords = iMostMatches AndAlso dfPercentMatched > dfHighestPercent) Then
                            topicBest = topic
                            dfHighestPercent = dfPercentMatched
                            iMostMatches = iMatchedKeywords
                        End If
                    End If

                    If bCommand Then
                        ' RE matching
                        For Each re As System.Text.RegularExpressions.Regex In GetRegularExpression(topic.Keywords.Trim.Replace("?", "\?"), sCommandOrSubject, False)
                            If re IsNot Nothing AndAlso re.IsMatch(sCommandOrSubject) Then
                                If PassRestrictions(topic.arlRestrictions) Then
                                    For i As Integer = 0 To 4
                                        Dim iRef As Integer = i + 1
                                        Dim sNumber As String = "%number" & iRef.ToString & "%"
                                        Dim sRefText As String = "%text" & iRef.ToString & "%"

                                        If topic.Keywords.Contains(sRefText) Then ' Needs full parsing really...
                                            Adventure.sReferencedText(i) = re.Match(sCommandOrSubject).Groups("text" & iRef.ToString).Value.Trim
                                            ' Update Refs
                                            For Each ref As clsNewReference In NewReferences
                                                If ref.ReferenceType = ReferencesType.Text AndAlso ref.ReferenceMatch = sRefText.Replace("%", "") Then
                                                    ref.Items.Clear()
                                                    Dim refItem As New clsSingleItem
                                                    refItem.bExplicitlyMentioned = True
                                                    refItem.MatchingPossibilities.Add(Adventure.sReferencedText(i))
                                                    refItem.sCommandReference = sCommandOrSubject
                                                    ref.Items.Add(refItem)
                                                    ref.sParentTask = "Topic"
                                                End If
                                            Next
                                        End If
                                    Next

                                    Return topic
                                End If
                            End If
                        Next
                    End If
                    If Not bAsk AndAlso Not bTell AndAlso Not bCommand Then
                        ' No matching whatsoever
                        If PassRestrictions(topic.arlRestrictions) Then Return topic
                    End If
                End If
            Next
            If topicBest IsNot Nothing Then Return topicBest
        End With
        Return Nothing
    End Function


    Private Sub ExecuteActions(ByVal Actions As ActionArrayList, Optional ByRef bTaskHasOutputNew As Boolean = False)

        DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.High, "Executing Actions...")
        iDebugIndent += 1
        For Each act As clsAction In Actions
            ExecuteSingleAction(act, "", Nothing, bTaskHasOutputNew)
        Next
        iDebugIndent -= 1

    End Sub
    Private Sub ExecuteActions(ByVal task As clsTask, Optional ByVal bCalledFromEvent As Boolean = False, Optional ByRef bTaskHasOutputNew As Boolean = False)
        DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Executing Actions...")
        iDebugIndent += 1
        For Each act As clsAction In task.arlActions
            ExecuteSingleAction(act, ParentTaskCommand(task), task, bCalledFromEvent, bTaskHasOutputNew)
        Next
        iDebugIndent -= 1
    End Sub

    Public Sub ShowUserSplash()
        If clsBlorb.Frontispiece > -1 Then
            Dim imgSplash As Byte() = Blorb.GetImage(clsBlorb.Frontispiece)
            If imgSplash IsNot Nothing Then
                Glue.ShowCoverArt(imgSplash)
            End If
        End If
    End Sub

    ' Returns the command from the parent task if we're a specific task.  Does this recursively in case we're specific of a specific etc.
    Public Function ParentTaskCommand(ByVal task As clsTask) As String
        While task.TaskType = clsTask.TaskTypeEnum.Specific
            task = Adventure.htblTasks(task.GeneralKey)
        End While
        If task.arlCommands.Count > 0 Then
            If task.arlCommands.Count > iMatchedTaskCommand Then
                Return task.arlCommands(iMatchedTaskCommand)
            Else
                Return task.arlCommands(0)
            End If
        Else
            Return ""
        End If
    End Function

    Private Function ToProper(ByVal sText As String, Optional ByVal bStrict As Boolean = False) As String
        Dim sReturn As String = Nothing

        If sText.Length > 0 Then sReturn = sText.Substring(0, 1).ToUpper
        If sText.Length > 1 Then
            If bStrict Then sReturn &= sText.Substring(1).ToLower Else sReturn &= sText.Substring(1)
        End If

        Return sReturn
    End Function


    Private Function AmbWord(ByVal sKeys As StringArrayList, ByVal RefType As ReferencesType) As String
        Dim bFoundInAllItemsSoFar As Boolean
        Dim bFoundInThisItem As Boolean

        AmbWord = Nothing

        ' Work out a common word that's in all the object names, and is also in the input
        For Each sWord As String In Split(sLastProperInput, " ")
            bFoundInAllItemsSoFar = True
            For Each sKey As String In sKeys
                bFoundInThisItem = False
                Select Case RefType
                    Case ReferencesType.Object
                        If Adventure.htblObjects.ContainsKey(sKey) Then
                            For Each sName As String In Adventure.htblObjects(sKey).arlNames
                                If sWord = sName Then
                                    bFoundInThisItem = True
                                    GoTo NextItem
                                End If
                            Next sName
                        End If
                    Case ReferencesType.Character
                        If Adventure.htblCharacters.ContainsKey(sKey) Then
                            If sWord = Adventure.htblCharacters(sKey).ProperName Then
                                bFoundInThisItem = True
                                GoTo NextItem
                            End If
                            For Each sName As String In Adventure.htblCharacters(sKey).arlDescriptors
                                If sWord = sName Then
                                    bFoundInThisItem = True
                                    GoTo NextItem
                                End If
                            Next sName
                        End If
                    Case ReferencesType.Location
                        If Adventure.htblLocations.ContainsKey(sKey) Then
                            For Each sName As String In Adventure.htblLocations(sKey).ShortDescription.ToString.ToLower.Split(" "c)
                                If sWord = sName Then
                                    bFoundInThisItem = True
                                    GoTo NextItem
                                End If
                            Next sName
                        End If
                End Select

                If Not bFoundInThisItem Then
                    bFoundInAllItemsSoFar = False
                    GoTo NextWord
                End If
NextItem:
            Next sKey
            If bFoundInAllItemsSoFar Then Return sWord
NextWord:
        Next sWord
    End Function

    Private Sub DisplayAmbiguityQuestion()
        NewReferences = Adventure.htblTasks(sAmbTask).NewReferencesWorking
        If NewReferences Is Nothing Then Exit Sub

        For iRef As Integer = 0 To NewReferences.Length - 1
            Debug.WriteLine("Reference " & iRef)
            With NewReferences(iRef)
                Debug.WriteLine("Number of Items in this Reference: " & .Items.Count)
                For Each itm As clsSingleItem In .Items
                    If itm.MatchingPossibilities.Count > 1 Then
                        Select Case .ReferenceType
                            Case ReferencesType.Object
                                Dim htblObs As New ObjectHashTable
                                For Each sKey As String In itm.MatchingPossibilities
                                    If Not htblObs.ContainsKey(sKey) Then htblObs.Add(Adventure.htblObjects(sKey), sKey)
                                Next
                                Dim bCanSeeAny As Boolean = False
                                For Each sKey As String In htblObs.Keys
                                    If Not bCanSeeAny AndAlso Adventure.Player.CanSeeObject(sKey) Then bCanSeeAny = True
                                Next
                                If Not bCanSeeAny Then     ' Want to try to move this into the library at some point, as we _may_ want to resolve ambiguous items that aren't visible to the player                               
                                    Dim bAnyPlural As Boolean = False
                                    For Each ob As clsObject In htblObs.Values
                                        If ob.IsPlural Then
                                            bAnyPlural = True
                                            Exit For
                                        End If
                                    Next
                                    If bAnyPlural Then
                                        Display("You can't see any " & AmbWord(itm.MatchingPossibilities, .ReferenceType) & "!" & vbCrLf)
                                    Else
                                        Display("You can't see any " & (New clsObject).GuessPluralFromNoun(AmbWord(itm.MatchingPossibilities, .ReferenceType)) & "!" & vbCrLf)
                                    End If
                                    sAmbTask = Nothing
                                Else
                                    Display("Which " & AmbWord(itm.MatchingPossibilities, .ReferenceType) & "?")
                                    Display(ToProper(htblObs.List("or")) & "." & vbCrLf)
                                End If
                                Exit Sub
                            Case ReferencesType.Character
                                Dim htblChars As New CharacterHashTable
                                For Each sKey As String In itm.MatchingPossibilities
                                    htblChars.Add(Adventure.htblCharacters(sKey), sKey)
                                Next
                                Dim bCanSeeAny As Boolean = False
                                For Each sKey As String In htblChars.Keys
                                    If Not bCanSeeAny AndAlso Adventure.Player.CanSeeCharacter(sKey) Then bCanSeeAny = True
                                Next
                                If Not bCanSeeAny Then
                                    Display("You can't see any " & AmbWord(itm.MatchingPossibilities, .ReferenceType) & "!" & vbCrLf)
                                    sAmbTask = Nothing
                                Else
                                    Display("Which " & AmbWord(itm.MatchingPossibilities, .ReferenceType) & "?")
                                    Display(ToProper(htblChars.List("or")) & "." & vbCrLf)
                                End If
                                Exit Sub
                            Case ReferencesType.Location
                                Dim htblLocs As New LocationHashTable
                                For Each sKey As String In itm.MatchingPossibilities
                                    htblLocs.Add(Adventure.htblLocations(sKey), sKey)
                                Next
                                Display("Which " & AmbWord(itm.MatchingPossibilities, .ReferenceType) & "?")
                                Display(ToProper(htblLocs.List("or")) & "." & vbCrLf)
                                Exit Sub
                            Case Else
                                ErrMsg("Unable to disambiguate reference types " & .ReferenceType.ToString)
                        End Select
                    ElseIf itm.MatchingPossibilities.Count = 0 Then
                        Display("Sorry, that does not clarify the ambiguity." & vbCrLf)
                        sAmbTask = Nothing
                        Exit Sub
                    End If
                Next
            End With
        Next
    End Sub

    Private Sub PrintOutReferences()

        If NewReferences Is Nothing Then Exit Sub
        For iRef As Integer = 0 To NewReferences.Length - 1
            Select Case iRef
                Case 0
                    DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, "First Reference: ", False)
                Case 1
                    DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, "Second Reference: ", False)
                Case 2
                    DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, "Third Reference: ", False)
                Case 3
                    DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, "Fourth Reference: ", False)
                Case Else
                    DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, "Reference " & iRef & ": ", False)
            End Select

            Dim iCount As Integer = 0
            If NewReferences(iRef) IsNot Nothing Then
                For Each itm As clsSingleItem In NewReferences(iRef).Items
                    For Each sKey As String In itm.MatchingPossibilities
                        If sKey IsNot Nothing Then
                            Select Case NewReferences(iRef).ReferenceType
                                Case ReferencesType.Object
                                    If Adventure.htblObjects.ContainsKey(sKey) Then DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, Adventure.htblObjects(sKey).FullName, False)
                                Case ReferencesType.Direction
                                    DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, sKey, False)
                                Case ReferencesType.Character
                                    If Adventure.htblCharacters.ContainsKey(sKey) Then DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, Adventure.htblCharacters(sKey).ProperName, False)
                                Case ReferencesType.Number

                                Case ReferencesType.Text

                            End Select
                            iCount += 1
                            If iCount < NewReferences(iRef).Items.Count Then DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, ", ", False)
                        Else
                            DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, "NULL reference", False)
                        End If
                    Next
                Next
            Else
                DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, "Nothing", False)
            End If
            DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, "")
        Next
    End Sub

    Private Sub GrabIt()

        Try
            Dim sNewIt As String = ""
            Dim sNewThem As String = ""
            Dim sNewHim As String = ""
            Dim sNewHer As String = ""
            Dim sWords() As String = sInput.Split(" "c)
            Dim PossibleItKeys As New StringArrayList
            Dim PossibleThemKeys As New StringArrayList
            Dim PossibleHimKeys As New StringArrayList
            Dim PossibleHerKeys As New StringArrayList

            ' First, look at anything visible, then seen
            For iScope As eScope = eScope.Visible To eScope.Seen
                For Each sWord As String In sWords

                    Dim htblObs As ObjectHashTable = Nothing
                    Dim htblChars As CharacterHashTable = Nothing
                    If iScope = eScope.Visible Then
                        htblObs = Adventure.htblObjects.VisibleTo(Adventure.Player.Key)
                        htblChars = Adventure.htblCharacters.VisibleTo(Adventure.Player.Key)
                    Else
                        htblObs = Adventure.htblObjects.SeenBy(Adventure.Player.Key)
                        htblChars = Adventure.htblCharacters.SeenBy(Adventure.Player.Key)
                    End If

                    If sNewIt = "" Then
                        For Each ob As clsObject In htblObs.Values
                            For Each sName As String In ob.arlNames
                                If sWord = sName Then
                                    If Not PossibleItKeys.Contains(ob.Key) Then PossibleItKeys.Add(ob.Key)
                                End If
                            Next
                        Next ob
                    End If
                    If sNewThem = "" Then
                        For Each ob As clsObject In htblObs.Values
                            If ob.IsPlural Then
                                For Each sName As String In ob.arlNames
                                    If sWord = sName Then
                                        If Not PossibleThemKeys.Contains(ob.Key) Then PossibleThemKeys.Add(ob.Key)
                                    End If
                                Next
                            End If
                        Next ob
                    End If
                    For Each ch As clsCharacter In htblChars.Values
                        Dim bMatch As Boolean = False
                        If sWord = ch.ProperName.ToLower Then bMatch = True
                        For Each sName As String In ch.arlDescriptors
                            If sWord = sName Then
                                bMatch = True
                                Exit For
                            End If
                        Next
                        If bMatch Then
                            Select Case ch.Gender
                                Case clsCharacter.GenderEnum.Male
                                    If Not PossibleItKeys.Contains(ch.Key) Then PossibleHimKeys.Add(ch.Key)
                                Case clsCharacter.GenderEnum.Female
                                    If Not PossibleItKeys.Contains(ch.Key) Then PossibleHerKeys.Add(ch.Key)
                                Case clsCharacter.GenderEnum.Unknown
                                    If Not PossibleItKeys.Contains(ch.Key) Then PossibleItKeys.Add(ch.Key)
                            End Select
                        End If
                    Next
                Next sWord

                If sNewIt = "" Then
                    If PossibleItKeys.Count = 1 Then
                        If Adventure.htblObjects.ContainsKey(PossibleItKeys(0)) Then
                            sNewIt = Adventure.htblObjects(PossibleItKeys(0)).FullName(ArticleTypeEnum.Definite)
                        Else
                            sNewIt = Adventure.htblCharacters(PossibleItKeys(0)).Name
                        End If
                    ElseIf PossibleItKeys.Count > 1 Then
                        Dim arlKeys As New StringArrayList

                        For Each sKey As String In PossibleItKeys
                            If Adventure.htblObjects.ContainsKey(sKey) Then
                                Dim ob As clsObject = Adventure.htblObjects(sKey)
                                For Each sPrefixWord As String In ob.Prefix.Split(" "c)
                                    For Each sWord As String In sWords
                                        If sPrefixWord = sWord Then
                                            arlKeys.Add(sKey)
                                            GoTo NextOb
                                        End If
                                    Next
                                Next
                            End If
NextOb:
                            If Adventure.htblCharacters.ContainsKey(sKey) Then
                                Dim ch As clsCharacter = Adventure.htblCharacters(sKey)
                                For Each sPrefixWord As String In ch.Prefix.Split(" "c)
                                    For Each sWord As String In sWords
                                        If sPrefixWord = sWord Then
                                            arlKeys.Add(sKey)
                                            GoTo NextChar
                                        End If
                                    Next
                                Next
                            End If
NextChar:
                        Next

                        If arlKeys.Count = 1 Then
                            If Adventure.htblObjects.ContainsKey(arlKeys(0)) Then
                                sNewIt = Adventure.htblObjects(arlKeys(0)).FullName(ArticleTypeEnum.Definite)
                            ElseIf Adventure.htblCharacters.ContainsKey(arlKeys(0)) Then
                                sNewIt = Adventure.htblCharacters(arlKeys(0)).Name
                            End If
                        End If
                    End If
                End If

                If sNewThem = "" Then
                    If PossibleThemKeys.Count = 1 Then
                        If Adventure.htblObjects.ContainsKey(PossibleThemKeys(0)) Then
                            sNewThem = Adventure.htblObjects(PossibleThemKeys(0)).FullName(ArticleTypeEnum.Definite)
                        Else
                            sNewThem = Adventure.htblCharacters(PossibleThemKeys(0)).Name
                        End If
                    ElseIf PossibleThemKeys.Count > 1 Then
                        Dim arlKeys As New StringArrayList

                        For Each sKey As String In PossibleThemKeys
                            If Adventure.htblObjects.ContainsKey(sKey) Then
                                Dim ob As clsObject = Adventure.htblObjects(sKey)
                                If ob.IsPlural Then
                                    For Each sPrefixWord As String In ob.Prefix.Split(" "c)
                                        For Each sWord As String In sWords
                                            If sPrefixWord = sWord Then
                                                arlKeys.Add(sKey)
                                                GoTo NextObT
                                            End If
                                        Next
                                    Next
                                End If
                            End If
NextObT:
                            If Adventure.htblCharacters.ContainsKey(sKey) Then
                                Dim ch As clsCharacter = Adventure.htblCharacters(sKey)
                                For Each sPrefixWord As String In ch.Prefix.Split(" "c)
                                    For Each sWord As String In sWords
                                        If sPrefixWord = sWord Then
                                            arlKeys.Add(sKey)
                                            GoTo NextCharT
                                        End If
                                    Next
                                Next
                            End If
NextCharT:
                        Next

                        If arlKeys.Count = 1 Then
                            If Adventure.htblObjects.ContainsKey(arlKeys(0)) Then
                                sNewThem = Adventure.htblObjects(arlKeys(0)).FullName(ArticleTypeEnum.Definite)
                            ElseIf Adventure.htblCharacters.ContainsKey(arlKeys(0)) Then
                                sNewThem = Adventure.htblCharacters(arlKeys(0)).Name
                            End If
                        End If
                    End If
                End If

                If sNewHim = "" Then
                    If PossibleHimKeys.Count = 1 Then
                        If Adventure.htblCharacters.ContainsKey(PossibleHimKeys(0)) Then
                            sNewHim = Adventure.htblCharacters(PossibleHimKeys(0)).Name(, False, False, ArticleTypeEnum.Definite)
                        End If
                    ElseIf PossibleHimKeys.Count > 1 Then
                        Dim arlKeys As New StringArrayList

                        For Each sKey As String In PossibleHimKeys
                            If Adventure.htblCharacters.ContainsKey(sKey) Then
                                Dim ch As clsCharacter = Adventure.htblCharacters(sKey)
                                For Each sPrefixWord As String In ch.Prefix.Split(" "c)
                                    For Each sWord As String In sWords
                                        If sPrefixWord = sWord Then
                                            arlKeys.Add(sKey)
                                            GoTo NextChar1
                                        End If
                                    Next
                                Next
                            End If
NextChar1:
                        Next

                        If arlKeys.Count = 1 Then
                            If Adventure.htblCharacters.ContainsKey(arlKeys(0)) Then
                                sNewHim = Adventure.htblCharacters(arlKeys(0)).Name(, False, False, ArticleTypeEnum.Definite)
                            End If
                        End If
                    End If
                End If

                If sNewHer = "" Then
                    If PossibleHerKeys.Count = 1 Then
                        If Adventure.htblCharacters.ContainsKey(PossibleHerKeys(0)) Then
                            sNewHer = Adventure.htblCharacters(PossibleHerKeys(0)).Name(, False, False, ArticleTypeEnum.Definite)
                        End If
                    ElseIf PossibleHerKeys.Count > 1 Then
                        Dim arlKeys As New StringArrayList

                        For Each sKey As String In PossibleHerKeys
                            If Adventure.htblCharacters.ContainsKey(sKey) Then
                                Dim ch As clsCharacter = Adventure.htblCharacters(sKey)
                                For Each sPrefixWord As String In ch.Prefix.Split(" "c)
                                    For Each sWord As String In sWords
                                        If sPrefixWord = sWord Then
                                            arlKeys.Add(sKey)
                                            GoTo NextChar2
                                        End If
                                    Next
                                Next
                            End If
NextChar2:
                        Next

                        If arlKeys.Count = 1 Then
                            If Adventure.htblCharacters.ContainsKey(arlKeys(0)) Then
                                sNewHer = Adventure.htblCharacters(arlKeys(0)).Name(, False, False, ArticleTypeEnum.Definite)
                            End If
                        End If
                    End If
                End If

            Next

            If sNewIt <> "" Then sIt = sNewIt
            If sNewThem <> "" Then sThem = sNewThem
            If sNewHim <> "" Then sHim = sNewHim
            If sNewHer <> "" Then sHer = sNewHer

            If sIt = "" Then sIt = "Absolutely Nothing"
            If sThem = "" Then sThem = "Absolutely Nothing"
            If sHim = "" Then sHim = "No male"
            If sHer = "" Then sHer = "No female"

        Catch ex As Exception
            ErrMsg("Error grabbing ""it""", ex)
            sIt = "Absolutely Nothing"
        End Try

    End Sub

    Private Sub ReplaceWord(ByRef sText As String, ByVal sFind As String, ByVal sReplace As String)
        sText = sText.Replace(" " & sFind & " ", " " & sReplace & " ")
        If sText.StartsWith(sFind & " ") Then sText = sReplace & " " & sRight(sText, sText.Length - (sFind.Length + 1))
        If sText.EndsWith(" " & sFind) Then sText = sLeft(sText, sText.Length - (sFind.Length + 1)) & " " & sReplace
        If sText = sFind Then sText = sReplace
    End Sub


    Friend Sub Restart()
        OpenAdventure(Adventure.FullPath)
    End Sub


    Friend Function SaveGame(Optional ByVal bSaveAs As Boolean = False) As Boolean
        Try
            Dim sFilename As String = Glue.QuerySavePath()

            If sFilename <> "" Then
                If Not sFilename.ToLower.EndsWith(".tas") Then sFilename &= ".tas"
                Adventure.sGameFilename = sFilename
                Dim ss As New StateStack
                If SaveState(ss.GetState, sFilename) Then
                    Display("Game """ & IO.Path.GetFileNameWithoutExtension(sFilename) & """ saved")
                    SaveSetting("ADRIFT", "Runner", "Game Path", IO.Path.GetDirectoryName(sFilename))
                    Adventure.Changed = False
                    Return True
                Else
                    Display("Error saving game")
                    Return False
                End If
            End If
        Catch ex As Exception
            Display("Error saving game: " & ex.Message)
            Return False
        End Try
        Return False
    End Function

    Friend Sub Restore(Optional sFilename As String = "")
        If sFilename <> "" Then
            If Not sFilename.ToLower.EndsWith(".tas") Then sFilename &= ".tas"
        Else
            sFilename = Glue.QueryRestorePath()
        End If

        If sFilename <> "" Then
            If IO.File.Exists(sFilename) Then
                States.Clear()
                For Each t As clsTask In Adventure.htblTasks.Values
                    t.Completed = False ' Just in case the save file doesn't cover that task
                Next
                Dim ss As New StateStack
                ss.LoadState(sFilename)
                Display("Game """ & IO.Path.GetFileNameWithoutExtension(sFilename) & """ restored" & vbCrLf, True)
                Adventure.sGameFilename = sFilename
                Adventure.eGameState = clsAction.EndGameEnum.Running
                Adventure.bDisplayedWinLose = False
                UpdateStatusBar()
                Display(Adventure.htblLocations(Adventure.Player.Location.LocationKey).ViewLocation, True)
                'If States.Count = 0 Then States.RecordState() ' So we're able to Undo immediately after a restore
                bSystemTask = False ' Allow events to run
                PrepareForNextTurn()
                UserSession.Map.RecalculateNode(Adventure.Map.FindNode(Adventure.Player.Location.LocationKey))
                UserSession.Map.SelectNode(Adventure.Player.Location.LocationKey)
            Else
                Display("Save file not found.", True)
            End If
        End If
    End Sub


    Friend bQuitting As Boolean = False
    Friend Function Quit(Optional ByVal bJustGame As Boolean = False) As Boolean
        If Adventure IsNot Nothing AndAlso Adventure.eGameState = clsAction.EndGameEnum.Running Then
            If Adventure.Changed Then
                Select Case Glue.QuerySaveBeforeQuit()
                    Case QueryResult.YES
                        SaveGame(True)
                    Case QueryResult.NO
                        ' Continue
                    Case QueryResult.CANCEL
                        Return False
                End Select
            End If
        End If

        If bJustGame Then
            Adventure.eGameState = clsAction.EndGameEnum.Neutral
        Else
            If Not bQuitting Then fRunner.Close()
        End If
        Return True
    End Function

    Friend Sub Undo()
        If States.SetLastState Then
            Adventure.bDisplayedWinLose = False
            UserSession.Map.RecalculateNode(Adventure.Map.FindNode(Adventure.Player.Location.LocationKey))
            UserSession.Map.SelectNode(Adventure.Player.Location.LocationKey)
            UpdateStatusBar()
            Dim sText As String = sTurnOutput
            Display("Undone.", , , False)
            If sText <> "" Then Display(sText)
        Else
            Display("Sorry, <c>undo</c> is not currently available.")
        End If
        bSystemTask = True
    End Sub

    Private Function GetBlock(ByVal sText As String) As String
        Dim cText() As Char = sText.ToCharArray
        Dim sOut As String = ""
        Dim iLevel As Integer = 0

        For Each c As Char In cText
            Select Case c
                Case "{"c, "["c
                    iLevel += 1
                Case "}"c, "]"c
                    iLevel -= 1
            End Select
            sOut &= c
            If iLevel = 0 Then Return sOut
        Next
        Return sText
    End Function

    Private Function MySplit(ByVal sText As String, ByRef iMinLength As Integer) As StringArrayList

        Dim sSplits() As String = Split(sText, "/")
        Dim sal As New StringArrayList
        Dim iLevel As Integer = 0
        Dim s As String = ""

        For Each sSplit As String In sSplits
            If iLevel > 0 Then s &= "/"
            s &= sSplit
            For i As Integer = 0 To sSplit.Length - 1
                Select Case sSplit(i)
                    Case "["c, "{"c
                        iLevel += 1
                    Case "]"c, "}"c
                        iLevel -= 1
                End Select
            Next
            If iLevel = 0 Then
                sal.Add(s)
                If s.Length < iMinLength Then iMinLength = s.Length
                s = ""
            End If
        Next
        Return sal
    End Function


    ' This should split any block, e.g. [examine/ex/x/look {at/in/under}] %object%
    ' into:
    ' examine %object%
    ' ex %object%
    ' x %object%
    ' look %object%
    ' look at %object%
    ' look in %object%
    ' look under %object%
    '
    ' [get/take/pick {up}] {the} hat {from Grandad}
    Private Function ExpandBlock(ByVal sBlock As String) As StringArrayList
        Dim sal As New StringArrayList

        If sBlock = "" Then
            sal.Add("")
            Return sal
        End If

        Dim sPre As String = System.Text.RegularExpressions.Regex.Replace(sBlock, "({|\[).*$", "")
        Dim sMid As String = GetBlock(sRight(sBlock, sBlock.Length - sPre.Length))
        Dim salPost As StringArrayList = Nothing

        If sMid <> "" Then
            Dim bOptional As Boolean = False
            If sLeft(sMid, 1) = "{" Then
                salPost = ExpandBlock(sRight(sBlock, sBlock.Length - sMid.Length - sPre.Length))
                For Each sPost As String In salPost
                    sal.Add(sPre & sPost)
                Next
                bOptional = True
            End If
            sMid = Mid(sMid, 2, sMid.Length - 2)

            Dim iMinLength As Integer = Integer.MaxValue
            Dim sSplits As StringArrayList = MySplit(sMid, iMinLength)
            If salPost Is Nothing AndAlso sPre.Length + iMinLength <= fRunner.txtInput.TextLength - 2 Then
                salPost = ExpandBlock(sRight(sBlock, sBlock.Length - (sMid.Length + 2) - sPre.Length))
            End If

            For Each sSplit As String In sSplits
                Dim salSplit As StringArrayList = ExpandBlock(sSplit)
                For Each s As String In salSplit
                    If bOptional Then s = "@@@" & s
                    If salPost IsNot Nothing Then
                        For Each sPost As String In salPost
                            If s <> "" AndAlso sPost <> "" AndAlso Not s.EndsWith(" ") AndAlso Not sPost.StartsWith(" ") Then sPost = " " & sPost
                            sal.Add(sPre & s & sPost)
                            If sal.Count > 1000 Then Return sal ' PK Girl can return millions of possibilities - Yikes, need to redesign!
                        Next
                    Else
                        sal.Add(sPre & s)
                    End If
                Next
            Next
        Else
            salPost = ExpandBlock(sRight(sBlock, sBlock.Length - sMid.Length - sPre.Length))
            For Each sPost As String In salPost
                sal.Add(sPre & sPost)
                If sal.Count > 1000 Then Return sal
            Next
        End If
        Return sal
    End Function

    Private Function EvaluateInput(ByVal iMinimumPriority As Integer, ByVal bPassingOnly As Boolean) As String
        Dim cCursor As Char = "Ø"c
        Dim cCommentCursor As Char = "@"c
        Dim sCursorFont As String = "Wingdings"
        Dim bComment As Boolean = False
        bComment = (fRunner.txtInput.Text.Length > 0 AndAlso fRunner.txtInput.Text(0) = cCommentCursor)

        InitialiseInputBox()
        If bComment Then
            Display("<c><font face=""" & sCursorFont & """ size=18>" & cCommentCursor & "</font> " & sInput & "</c>" & vbCrLf, True, False)
            Return ""
        End If

        If iMinimumPriority = 0 Then
            Display("<c><font face=""" & sCursorFont & """ size=14>" & cCursor & "</font> " & sInput & "</c>" & vbCrLf, True, False, False)
            sInput = sInput.ToLower
            If ContainsWord(sInput, "it") Then
                Display("<c>(" & sIt & ")</c>" & vbCrLf, True)
                ReplaceWord(sInput, "it", sIt)
            End If
            If ContainsWord(sInput, "them") Then
                Display("<c>(" & sThem & ")</c>" & vbCrLf, True)
                ReplaceWord(sInput, "them", sThem)
            End If
            If ContainsWord(sInput, "him") Then
                Display("<c>(" & sHim & ")</c>" & vbCrLf, True)
                ReplaceWord(sInput, "him", sHim)
            End If
            If ContainsWord(sInput, "her") Then
                Display("<c>(" & sHer & ")</c>" & vbCrLf, True)
                ReplaceWord(sInput, "her", sHer)
            End If
            If (sInput = "again" OrElse sInput = "g") AndAlso salCommands.Count > 2 Then
                Display("<c>(" & salCommands(salCommands.Count - 3) & ")</c>" & vbCrLf, True)
                sInput = salCommands(salCommands.Count - 3)
                salCommands.RemoveAt(salCommands.Count - 2) ' Don't store 'again'
            End If
            GrabIt()

            Dim sPreSyn As String = sInput
            For Each syn As clsSynonym In Adventure.htblSynonyms.Values
                For Each sFrom As String In syn.ChangeFrom
                    If ContainsWord(sInput, sFrom) Then
                        ReplaceWord(sInput, sFrom, syn.ChangeTo)
                    End If
                Next
            Next
            If sInput <> sPreSyn Then
                DebugPrint(ItemEnum.General, "", DebugDetailLevelEnum.Medium, "Synonyms changed input """ & sPreSyn & """ to """ & sInput & """")
            End If
            If CBool(GetSetting("ADRIFT", "Runner", "BlankLine", "0")) Then Display(vbCrLf)
        End If

        sInput = sInput.ToLower

        ' Don't actually respond to the tasks here, in case the user has created a task to override the system one.
        If Adventure.eGameState <> clsAction.EndGameEnum.Running OrElse Adventure.dVersion < 5 Then
            If SystemTasks(True) Then
                Return ""
            Else
                If Adventure.eGameState <> clsAction.EndGameEnum.Running Then
                    Display("Please give one of the answers above." & vbCrLf)
                    Return ""
                End If
            End If
        End If

        Dim sTaskKey As String = Nothing

        If sAmbTask IsNot Nothing Then
            sTaskKey = ResolveAmbiguity(sInput)

            If Not sTaskKey Is Nothing Then
                sRestrictionTextMulti = ""
                sRestrictionText = sRestrictionTextMulti
            End If
        Else
            sLastProperInput = sInput
            Erase NewReferences
        End If

        ' only run this if we're not resolving an ambiguity or it left no tasks
        If sTaskKey Is Nothing Then
            Dim cRefCopy(-1) As clsNewReference
            Dim bCopied As Boolean = False
            Dim sRememberAmbTask As String = Nothing
            For i As Integer = 0 To 4
                Adventure.sReferencedText(i) = ""
            Next
            ' If Not sAmbTask Is Nothing Then remember References, so we can reapply them if we don't get a new command
            If Not sAmbTask Is Nothing Then
                ReDim cRefCopy(NewReferences.Length - 1)
                Array.Copy(NewReferences, cRefCopy, NewReferences.Length)
                bCopied = True
                sRememberAmbTask = sAmbTask
            End If
            sTaskKey = GetGeneralTask(sInput, iMinimumPriority, False)
            If Adventure.sReferencedText(0) = "" Then Adventure.sReferencedText(0) = sInput
            If sTaskKey Is Nothing AndAlso sAmbTask IsNot Nothing AndAlso iMinimumPriority > 0 AndAlso sOutputText <> "" Then sAmbTask = Nothing ' Suppress ambiguity question if we're running further down task list and we already have a failure response
            If sTaskKey Is Nothing AndAlso sAmbTask Is Nothing AndAlso Not sRememberAmbTask Is Nothing Then sAmbTask = sRememberAmbTask
            If sTaskKey Is Nothing AndAlso Not sAmbTask Is Nothing AndAlso bCopied Then
                ReDim NewReferences(cRefCopy.Length - 1)
                Array.Copy(cRefCopy, NewReferences, cRefCopy.Length)
            End If

        End If

        If Not sAmbTask Is Nothing AndAlso sTaskKey Is Nothing Then
            ' Display ambiguity question
            DisplayAmbiguityQuestion()
        Else
            sAmbTask = Nothing
            If sTaskKey IsNot Nothing Then
                Dim taskRun As clsTask = Adventure.htblTasks(sTaskKey)
                NewReferences = taskRun.NewReferencesWorking
                AttemptToExecuteTask(sTaskKey, , , , , , , bPassingOnly)
                If taskRun.bSystemTask Then bSystemTask = True

                While Adventure.qTasksToRun.Count > 0
                    Dim sKey As String = Adventure.qTasksToRun.Dequeue
                    AttemptToExecuteTask(sKey, True)
                End While

                If iMinimumPriority = 0 Then
                    If sOutputText = "" Then
                        NotUnderstood()
                    Else
                        ' Ok, successful task.  So long as we didn't execute a system task
                        If Not bSystemTask Then TurnBasedStuff()
                        Adventure.Changed = True
                        sRememberedVerb = ""
                    End If
                End If
            Else
                If iMinimumPriority = 0 Then
                    If Adventure.eGameState = clsAction.EndGameEnum.Running Then
                        If Not SystemTasks() Then NotUnderstood()
                        bSystemTask = True
                    End If
                End If
            End If
            If iMinimumPriority = 0 AndAlso sOutputText <> "***SYSTEM***" Then Display(vbCrLf)
        End If

        If sOutputText <> "***SYSTEM***" Then
            If iMinimumPriority = 0 Then
                Display(vbCrLf, True)
                DebugPrint(ItemEnum.General, "", DebugDetailLevelEnum.Low, "ENDOFTURN")
            End If
            Debug.WriteLine(Now.Subtract(dtDebug).ToString())

            UpdateStatusBar()
        End If

        Return sOutputText
    End Function


    Private Class TaskSorter
        Implements System.Collections.Generic.IComparer(Of clsTask)

        Public Function Compare(x As clsTask, y As clsTask) As Integer Implements System.Collections.Generic.IComparer(Of clsTask).Compare
            Return x.Priority.CompareTo(y.Priority)
        End Function
    End Class

    Private Sub NotUnderstood()

        If sRememberedVerb <> "" Then
            sInput = sRememberedVerb & " " & sInput
            sRememberedVerb = ""
            EvaluateInput(-1, False)
            If sOutputText <> "" Then Exit Sub
        End If

        With Adventure
            If .lstTasks Is Nothing Then
                .lstTasks = New List(Of clsTask)
                For Each tas As clsTask In .htblTasks.Values
                    .lstTasks.Add(tas)
                Next
                .lstTasks.Sort(New TaskSorter)
            End If
            If .listKnownWords Is Nothing Then
                .listKnownWords = New List(Of String)
                ' Verbs
                For Each tas As clsTask In .lstTasks
                    If tas.TaskType = clsTask.TaskTypeEnum.General Then
                        For Each sCommand As String In tas.arlCommands
                            sCommand = sCommand.Replace("[", " ").Replace("]", " ").Replace("{", " ").Replace("}", " ").Replace("/", " ")
                            While sCommand.Contains("  ")
                                sCommand = sCommand.Replace("  ", " ")
                            End While
                            For Each sWord As String In sCommand.Split(" "c)
                                If Not .listKnownWords.Contains(sWord) Then .listKnownWords.Add(sWord)
                            Next
                        Next
                    End If
                Next
                ' Nouns
                For Each ob As clsObject In .htblObjects.Values
                    If Not .listKnownWords.Contains(ob.Article) Then .listKnownWords.Add(ob.Article)
                    For Each sWord As String In ob.Prefix.Split(" "c)
                        If Not .listKnownWords.Contains(sWord) Then .listKnownWords.Add(sWord)
                    Next
                    For Each sWord As String In ob.arlNames
                        If Not .listKnownWords.Contains(sWord) Then .listKnownWords.Add(sWord)
                    Next
                Next
                For Each ch As clsCharacter In .htblCharacters.Values
                    If Not .listKnownWords.Contains(ch.Article) Then .listKnownWords.Add(ch.Article)
                    If ch.Prefix IsNot Nothing Then
                        For Each sWord As String In ch.Prefix.Split(" "c)
                            If Not .listKnownWords.Contains(sWord) Then .listKnownWords.Add(sWord)
                        Next
                    End If
                    For Each sWord As String In ch.arlDescriptors
                        If Not .listKnownWords.Contains(sWord) Then .listKnownWords.Add(sWord)
                    Next
                    If Not .listKnownWords.Contains(ch.ProperName) Then .listKnownWords.Add(ch.ProperName.ToLower)
                Next
                ' Adverbs
                For Each sWord As String In sDirectionsRE.Split("|"c)
                    If Not .listKnownWords.Contains(sWord) Then .listKnownWords.Add(sWord)
                Next
                ' Additional words
                For Each sWord As String In New String() {"and"}
                    If Not .listKnownWords.Contains(sWord) Then .listKnownWords.Add(sWord)
                Next
            End If

            ' Check all words typed, to ensure they are valid
            For Each sWord As String In sInput.Split(" "c)
                If Not .listKnownWords.Contains(sWord) Then
                    Display("I did not understand the word """ & sWord & """.")
                    Exit Sub
                End If
            Next

            ' If the user just entered a verb, check to see if we understand verb noun
            If Not sInput.Contains(" ") Then
                For Each tas As clsTask In .lstTasks 'Adventure.htblTasks.Values
                    If tas.TaskType = clsTask.TaskTypeEnum.General Then
                        If tas.arlCommands(0).Contains(sInput) AndAlso tas.arlCommands(0).Contains(" ") Then
                            If tas.arlCommands(0).Contains("%object") Then
                                For Each lRegEx As List(Of System.Text.RegularExpressions.Regex) In tas.RegExs
                                    For Each RegEx As System.Text.RegularExpressions.Regex In lRegEx
                                        If RegEx.IsMatch(sInput & " sdkfjdslkj") Then
                                            sRememberedVerb = sInput
                                            Display(PCase(sInput) & " what?")
                                            Exit Sub
                                        End If
                                    Next
                                Next
                            ElseIf tas.arlCommands(0).Contains("%character") Then
                                For Each lRegEx As List(Of System.Text.RegularExpressions.Regex) In tas.RegExs
                                    For Each RegEx As System.Text.RegularExpressions.Regex In lRegEx
                                        If RegEx.IsMatch(sInput & " sdkfjdslkj") Then
                                            sRememberedVerb = sInput
                                            Display(PCase(sInput) & " who?")
                                            Exit Sub
                                        End If
                                    Next
                                Next
                            ElseIf tas.arlCommands(0).Contains("%direction%") Then
                                For Each lRegEx As List(Of System.Text.RegularExpressions.Regex) In tas.RegExs
                                    For Each RegEx As System.Text.RegularExpressions.Regex In lRegEx
                                        If RegEx.IsMatch(sInput & " " & DirectionName(DirectionsEnum.North)) Then
                                            sRememberedVerb = sInput
                                            Display(PCase(sInput) & " where?")
                                            Exit Sub
                                        End If
                                    Next
                                Next
                            End If
                        End If
                    End If
                Next
            End If

            ' If the user just entered a noun, give a default message
            For Each ob As clsObject In Adventure.htblObjects.Values
                If Adventure.Player.HasSeenObject(ob.Key) Then
                    If Adventure.Player.CanSeeObject(ob.Key) Then
                        Dim re As New System.Text.RegularExpressions.Regex(ob.sRegularExpressionString)
                        If re.IsMatch(sInput) Then
                            Display("I don't understand what you want to do with " & ob.FullName(ArticleTypeEnum.Definite) & ".")
                            Exit Sub
                        End If
                    Else
                        ' Hmm, should we give a response when mentioning objects seen but not visible
                    End If
                End If
            Next
            For Each ch As clsCharacter In Adventure.htblCharacters.Values
                If Adventure.Player.HasSeenCharacter(ch.Key) Then
                    If Adventure.Player.CanSeeCharacter(ch.Key) Then
                        Dim re As New System.Text.RegularExpressions.Regex(ch.sRegularExpressionString)
                        If re.IsMatch(sInput) Then
                            Display("I don't understand what you want to do with " & ch.Name & ".")
                            Exit Sub
                        End If
                    Else
                        ' Hmm, should we give a response when mentioning characters seen but not visible
                    End If
                End If
            Next
        End With

        Display(Adventure.NotUnderstood)
    End Sub

    Private Function SystemTasks(Optional ByVal bEarly As Boolean = False) As Boolean
        SystemTasks = True
        Select Case sInput
            Case "restart"
                Restart()
                If Not bEarly Then sOutputText = "***SYSTEM***"
            Case "restore"
                Restore()
                Display(vbCrLf & vbCrLf, True)
                If Not bEarly Then sOutputText = "***SYSTEM***"
            Case "save"
                If bEarly Then Return False Else SaveGame()
            Case "save as", "saveas"
                If bEarly Then Return False Else SaveGame(True)
            Case "quit"
                Quit(Not bEarly)
            Case "undo"
                Undo()
                If bEarly Then Display(vbCrLf & vbCrLf, True)
            Case "wait", "z"
                If bEarly Then
                    Return False
                Else
                    sOutputText = ReplaceALRs("Time passes...") ' TODO - Bring this into Developer as a task
                    For i As Integer = 0 To Adventure.WaitTurns - 1
                        TurnBasedStuff()
                    Next
                End If
            Case Else
                If bEarly Then Return False
                If sInput.StartsWith("save ") AndAlso CharacterCount(sInput, " "c) = 1 Then
                    Dim sFilename As String = sInput.Replace("save ", "")
                    If sFilename.StartsWith("""") AndAlso sFilename.EndsWith("""") Then sFilename = sFilename.Substring(1, sFilename.Length - 2)
                    For Each c As Char In IO.Path.GetInvalidPathChars
                        If sFilename.Contains(c) Then
                            Display("Bad Filename")
                            Return True
                        End If
                    Next
                    If Not sFilename.Contains(IO.Path.DirectorySeparatorChar) Then
                        If Adventure.sGameFilename <> "" Then
                            Dim sPath As String = IO.Path.GetDirectoryName(Adventure.sGameFilename)
                            sFilename = sPath & IO.Path.DirectorySeparatorChar & sFilename
                        Else
                            sFilename = GetSetting("ADRIFT", "Runner", "Game Path", System.Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments)) & "\" & sFilename
                        End If
                    End If
                    Adventure.sGameFilename = sFilename
                    SaveGame()
                    Return True
                End If

                If sInput.StartsWith("restore ") AndAlso CharacterCount(sInput, " "c) = 1 Then
                    Dim sFilename As String = sInput.Replace("restore ", "")
                    If sFilename.StartsWith("""") AndAlso sFilename.EndsWith("""") Then sFilename = sFilename.Substring(1, sFilename.Length - 2)
                    For Each c As Char In IO.Path.GetInvalidPathChars
                        If sFilename.Contains(c) Then
                            Display("Bad Filename")
                            Return True
                        End If
                    Next
                    If Not sFilename.Contains(IO.Path.DirectorySeparatorChar) Then
                        If Adventure.sGameFilename <> "" Then
                            Dim sPath As String = IO.Path.GetDirectoryName(Adventure.sGameFilename)
                            sFilename = sPath & IO.Path.DirectorySeparatorChar & sFilename
                        Else
                            sFilename = GetSetting("ADRIFT", "Runner", "Game Path", System.Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments)) & "\" & sFilename
                        End If
                    End If
                    Restore(sFilename)
                    Return True
                End If
                Return False
        End Select

    End Function

    Public Sub UpdateStatusBar()
        If Adventure Is Nothing Then Exit Sub

        Try
            Dim sDescription As String = ""
            Dim sScore As String = ""
            Dim sUserStatus As String = ""

            If Adventure.eGameState = clsAction.EndGameEnum.Running Then
                If Adventure.Player IsNot Nothing Then
                    If Adventure.Player.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.Hidden OrElse Adventure.Player.Location.LocationKey = "" Then
                        sDescription = "(Nowhere)"
                    Else
                        sDescription = Adventure.htblLocations(Adventure.Player.Location.LocationKey).ShortDescriptionSafe
                        If Adventure.Player.Location.ExistWhere <> clsCharacterLocation.ExistsWhereEnum.AtLocation Then
                            sDescription &= " (" & Adventure.Player.Location.ToString & ")"
                        End If
                    End If
                End If
                sUserStatus = Adventure.sUserStatus
            Else
                sDescription = "Game Over"
            End If

            If Adventure.MaxScore > 0 Then sScore = "Score: " & Adventure.Score

            If Not Adventure.Enabled(clsAdventure.EnabledOptionEnum.Score) Then sScore = ""
            fRunner.UpdateStatusBar(sDescription, sScore, sUserStatus)
        Catch exOD As ObjectDisposedException
            ' Fail silently
        Catch ex As Exception
            ErrMsg("UpdateStatusBar error", ex)
        End Try

    End Sub

    Friend bEventsRunning As Boolean = False
    Private Sub TurnBasedStuff()

        If Adventure.eGameState <> clsAction.EndGameEnum.Running Then Exit Sub

        For Each c As clsCharacter In Adventure.htblCharacters.Values
            For Each w As clsWalk In c.arlWalks
                If Adventure.eGameState <> clsAction.EndGameEnum.Running Then Exit Sub
                w.IncrementTimer()
            Next
        Next
        bEventsRunning = True
        For Each e As clsEvent In Adventure.htblEvents.Values
            If Adventure.eGameState <> clsAction.EndGameEnum.Running Then Exit Sub
            If e.EventType = clsEvent.EventTypeEnum.TurnBased Then e.IncrementTimer()
        Next
        ' Needs to be a separate loop in case a later event runs a task that starts an earlier event
        For Each e As clsEvent In Adventure.htblEvents.Values
            If e.EventType = clsEvent.EventTypeEnum.TurnBased Then e.bJustStarted = False
        Next
        bEventsRunning = False

    End Sub

    Public Sub TimeBasedStuff()
        If Adventure.eGameState <> clsAction.EndGameEnum.Running OrElse fRunner.Locked Then Exit Sub

        bEventsRunning = True

        For Each e As clsEvent In Adventure.htblEvents.Values
            If Adventure.eGameState <> clsAction.EndGameEnum.Running Then Exit For
            If e.EventType = clsEvent.EventTypeEnum.TimeBased Then e.IncrementTimer()
        Next
        ' Needs to be a separate loop in case a later event runs a task that starts an earlier event
        For Each e As clsEvent In Adventure.htblEvents.Values
            If e.EventType = clsEvent.EventTypeEnum.TimeBased Then e.bJustStarted = False
        Next
        bEventsRunning = False
        If sOutputText <> "" Then Display("", True)

        CheckEndOfGame()
    End Sub

    Private Sub PrepareForNextTurn()
        If Not bSystemTask AndAlso Adventure.eGameState = clsAction.EndGameEnum.Running Then States.RecordState()
        sTurnOutput = ""

        ' Mark objects as seen for all the characters
        For Each ch As clsCharacter In Adventure.htblCharacters.Values
            For Each ob As clsObject In Adventure.htblObjects.Values
                If ch.CanSeeObject(ob.Key) Then ob.SeenBy(ch.Key) = True
            Next
            Dim sLocKey As String = ch.Location.LocationKey
            If sLocKey <> HIDDEN AndAlso sLocKey <> "" Then
                Dim locChar As clsLocation = Adventure.htblLocations(sLocKey)
                If locChar IsNot Nothing Then locChar.SeenBy(ch.Key) = True
            End If

            For Each ch2 As clsCharacter In Adventure.htblCharacters.Values
                If Not ch.SeenBy(ch2.Key) AndAlso ch.CanSeeCharacter(ch2.Key) Then
                    ch.SeenBy(ch2.Key) = True
                End If
            Next
            ch.dictHasRouteCache.Clear()
            ch.dictRouteErrors.Clear()
            If ch.Introduced Then
                If ch IsNot Adventure.Player AndAlso Not Adventure.Player.CanSeeCharacter(ch.Key) Then
                    ch.Introduced = False
                End If
            End If
        Next
        For Each eGen As clsCharacter.GenderEnum In New clsCharacter.GenderEnum() {clsCharacter.GenderEnum.Male, clsCharacter.GenderEnum.Female, clsCharacter.GenderEnum.Unknown}
            Adventure.lCharMentionedThisTurn(eGen) = New Generic.List(Of String)
        Next

        htblCompleteableGeneralTasks.Clear()
        For Each tas As clsTask In Adventure.Tasks(clsAdventure.TasksListEnum.GeneralTasks).Values
            If tas.TaskType = clsTask.TaskTypeEnum.General AndAlso (Not tas.Completed OrElse tas.Repeatable) Then
                htblCompleteableGeneralTasks.Add(tas, tas.Key)
            End If
        Next

        iPrepProgress = 2

        htblVisibleObjects.Clear()
        htblSeenObjects.Clear()
        For Each ob As clsObject In Adventure.htblObjects.Values
            If ob.SeenBy(Adventure.Player.Key) Then htblSeenObjects.Add(ob, ob.Key)
            If ob.IsVisibleTo(Adventure.Player.Key) Then htblVisibleObjects.Add(ob, ob.Key)
            ob.PrevParent = ob.Parent
        Next
        For Each ch As clsCharacter In Adventure.htblCharacters.Values
            ch.PrevParent = ch.Parent
        Next

        PronounKeys.Clear()
    End Sub

    ' Runs this on a background thread so if user types a command we just resume where we are
    Private Sub CalculateCompleteableTasks()
        Dim htblTasks As New TaskHashTable

        Try
            ' TODO: This should only add if completeable in current room, i.e. pass all restrictions ignoring ones regarding references.
            For Each sKey As String In htblCompleteableGeneralTasks.Keys
                Dim task As clsTask = Adventure.htblTasks(sKey)
                If iPrepProgress = 1 OrElse task.HasReferences OrElse task.IsCompleteable Then htblTasks.Add(task, sKey)
            Next
        Catch
            htblTasks = htblCompleteableGeneralTasks
        End Try

        htblCompleteableGeneralTasks = htblTasks
        iPrepProgress = 2
    End Sub


    Private Function ResolveAmbiguity(ByVal sInput As String) As String
        Dim bResolved As Boolean = False

        ResolveAmbiguity = sAmbTask ' the default unless we're still ambiguous

        For iRef As Integer = 0 To NewReferences.Length - 1
            Debug.WriteLine("Reference " & iRef)
            If NewReferences(iRef) IsNot Nothing Then
                With NewReferences(iRef)
                    Debug.WriteLine("Number of Refs in this Reference: " & .Items.Count)
                    For i As Integer = 0 To .Items.Count - 1
                        Dim salRefs As StringArrayList = .Items(i).MatchingPossibilities
                        Debug.WriteLine("Number of matching references: " & salRefs.Count)
                        If salRefs.Count <> 1 Then
                            If Not bResolved Then
                                .Items(i).MatchingPossibilities = PossibleKeys(salRefs, sInput, .ReferenceType)
                            End If
                            If .Items(i).MatchingPossibilities.Count <> 1 Then Return Nothing
                            If .ReferenceType = ReferencesType.Object Then sIt = Adventure.htblObjects(.Items(i).MatchingPossibilities(0)).FullName(ArticleTypeEnum.Definite)
                            If .ReferenceType = ReferencesType.Object AndAlso Adventure.htblObjects(.Items(i).MatchingPossibilities(0)).IsPlural Then sThem = Adventure.htblObjects(.Items(i).MatchingPossibilities(0)).FullName(ArticleTypeEnum.Definite)
                            If .ReferenceType = ReferencesType.Character Then
                                With Adventure.htblCharacters(.Items(i).MatchingPossibilities(0))
                                    Select Case .Gender
                                        Case clsCharacter.GenderEnum.Male
                                            sHim = .Name
                                        Case clsCharacter.GenderEnum.Female
                                            sHer = .Name
                                        Case clsCharacter.GenderEnum.Unknown
                                            sIt = .Name
                                    End Select
                                End With
                            End If
                            bResolved = True
                        End If
                    Next
                End With
            End If
        Next
    End Function

    Private Function ContainsWord(ByVal sSentence As String, ByVal sCheckWord As String) As Boolean
        Dim sChecks() As String = Split(sCheckWord, " ")
        Dim sWords() As String = Split(sSentence.ToLower, " ")

        For Each sCheck As String In sChecks
            Dim bFound As Boolean = False
            For Each sWord As String In sWords
                If sWord = sCheck Then
                    bFound = True
                    Exit For
                End If
            Next
            If Not bFound Then Return False
        Next

        Return True
    End Function

    ' This takes a list of possible refs, then narrows the field (hopefully to 1) given the input
    Private Function PossibleKeys(ByVal salKeys As StringArrayList, ByVal sInput As String, ByVal sRefType As ReferencesType) As StringArrayList
        Dim salReturn As New StringArrayList
        ' Check each word in the refined text to ensure they all match

        Select Case sRefType
            Case ReferencesType.Object
                For Each sKey As String In salKeys
                    Dim ob As clsObject = Adventure.htblObjects(sKey)
                    Dim bMatchAll As Boolean = True
                    For Each sWord As String In sInput.Split(" "c)
                        Dim bWordInObject As Boolean = False
                        If sWord = "the" OrElse sWord = ob.Article OrElse ContainsWord(ob.Prefix, sWord) Then
                            bWordInObject = True
                        Else
                            For Each sName As String In ob.arlNames
                                If ContainsWord(sName, sWord) Then
                                    bWordInObject = True
                                    Exit For
                                End If
                            Next
                        End If
                        If Not bWordInObject Then
                            bMatchAll = False
                            Exit For
                        End If
                    Next
                    If bMatchAll Then salReturn.Add(sKey)
                Next

            Case ReferencesType.Character
                For Each sKey As String In salKeys
                    Dim ch As clsCharacter = Adventure.htblCharacters(sKey)
                    Dim bMatchAll As Boolean = True
                    For Each sWord As String In sInput.Split(" "c)
                        Dim bWordInChar As Boolean = False
                        If ContainsWord(ch.Prefix, sWord) OrElse ContainsWord(ch.ProperName, sWord) Then
                            bWordInChar = True
                        Else
                            For Each sName As String In ch.arlDescriptors
                                If ContainsWord(sName, sWord) Then
                                    bWordInChar = True
                                    Exit For
                                End If
                            Next
                        End If
                        If Not bWordInChar Then
                            bMatchAll = False
                            Exit For
                        End If
                    Next
                    If bMatchAll Then salReturn.Add(sKey)
                Next

            Case ReferencesType.Location
                For Each sKey As String In salKeys
                    Dim loc As clsLocation = Adventure.htblLocations(sKey)
                    Dim bMatchAll As Boolean = True
                    For Each sWord As String In sInput.Split(" "c)
                        Dim bWordInLoc As Boolean = False
                        If ContainsWord(loc.ShortDescription.ToString(True), sWord) Then
                            bWordInLoc = True
                        End If
                        If Not bWordInLoc Then
                            bMatchAll = False
                            Exit For
                        End If
                    Next
                    If bMatchAll Then salReturn.Add(sKey)
                Next

        End Select

        Return salReturn
    End Function


    Private Function GetReference(ByVal sReference As String) As String
        If NewReferences Is Nothing Then Return Nothing

        For iRef As Integer = 0 To NewReferences.Length - 1
            With NewReferences(iRef)
                If NewReferences(iRef) IsNot Nothing AndAlso .Items IsNot Nothing Then 'AndAlso .Items.Count > 0 AndAlso .Items(0).MatchingPossibilities.Count > 0 Then

                    Select Case sReference
                        Case "ReferencedObject", "ReferencedObject1", "ReferencedObject2", "ReferencedObject3", "ReferencedObject4", "ReferencedObject5", "ReferencedObjects"
                            If .ReferenceType = ReferencesType.Object Then
                                If sReference = "ReferencedObjects" _
                                    OrElse (.ReferenceMatch = "object1" AndAlso (sReference = "ReferencedObject" OrElse sReference = "ReferencedObject1")) _
                                    OrElse (.ReferenceMatch = "object2" AndAlso sReference = "ReferencedObject2") _
                                    OrElse (.ReferenceMatch = "object3" AndAlso sReference = "ReferencedObject3") _
                                    OrElse (.ReferenceMatch = "object4" AndAlso sReference = "ReferencedObject4") _
                                    OrElse (.ReferenceMatch = "object5" AndAlso sReference = "ReferencedObject5") Then
                                    If .Items.Count > 0 AndAlso .Items(0).MatchingPossibilities.Count > 0 Then
                                        Return .Items(0).MatchingPossibilities(0)
                                    Else
                                        Return Nothing
                                    End If
                                End If
                            End If
                        Case "ReferencedDirection", "ReferencedDirection1", "ReferencedDirection2", "ReferencedDirection3", "ReferencedDirection4", "ReferencedDirection5"
                            If .ReferenceType = ReferencesType.Direction Then
                                If (.ReferenceMatch = "direction1" AndAlso (sReference = "ReferencedDirection" OrElse sReference = "ReferencedDirection1")) _
                                    OrElse (.ReferenceMatch = "direction2" AndAlso sReference = "ReferencedDirection2") _
                                    OrElse (.ReferenceMatch = "direction3" AndAlso sReference = "ReferencedDirection3") _
                                    OrElse (.ReferenceMatch = "direction4" AndAlso sReference = "ReferencedDirection4") _
                                    OrElse (.ReferenceMatch = "direction5" AndAlso sReference = "ReferencedDirection5") Then
                                    If .Items.Count > 0 AndAlso .Items(0).MatchingPossibilities.Count > 0 Then
                                        Return .Items(0).MatchingPossibilities(0)
                                    Else
                                        Return Nothing
                                    End If
                                End If
                            End If
                        Case "ReferencedCharacter", "ReferencedCharacter1", "ReferencedCharacter2", "ReferencedCharacter3", "ReferencedCharacter4", "ReferencedCharacter5"
                            If .ReferenceType = ReferencesType.Character Then
                                If sReference = "ReferencedCharacters" _
                                    OrElse (.ReferenceMatch = "character1" AndAlso (sReference = "ReferencedCharacter" OrElse sReference = "ReferencedCharacter1")) _
                                    OrElse (.ReferenceMatch = "character2" AndAlso sReference = "ReferencedCharacter2") _
                                    OrElse (.ReferenceMatch = "character3" AndAlso sReference = "ReferencedCharacter3") _
                                    OrElse (.ReferenceMatch = "character4" AndAlso sReference = "ReferencedCharacter4") _
                                    OrElse (.ReferenceMatch = "character5" AndAlso sReference = "ReferencedCharacter5") Then
                                    If .Items.Count > 0 AndAlso .Items(0).MatchingPossibilities.Count > 0 Then
                                        Return .Items(0).MatchingPossibilities(0)
                                    Else
                                        Return Nothing
                                    End If
                                End If
                            End If
                        Case "ReferencedLocation", "ReferencedLocation1", "ReferencedLocation2", "ReferencedLocation3", "ReferencedLocation4", "ReferencedLocation5"
                            If .ReferenceType = ReferencesType.Location Then
                                If (.ReferenceMatch = "location1" AndAlso (sReference = "ReferencedLocation" OrElse sReference = "ReferencedLocation1")) _
                                    OrElse (.ReferenceMatch = "location2" AndAlso sReference = "ReferencedLocation2") _
                                    OrElse (.ReferenceMatch = "location3" AndAlso sReference = "ReferencedLocation3") _
                                    OrElse (.ReferenceMatch = "location4" AndAlso sReference = "ReferencedLocation4") _
                                    OrElse (.ReferenceMatch = "location5" AndAlso sReference = "ReferencedLocation5") Then
                                    If .Items.Count > 0 AndAlso .Items(0).MatchingPossibilities.Count > 0 Then
                                        Return .Items(0).MatchingPossibilities(0)
                                    Else
                                        Return Nothing
                                    End If
                                End If
                            End If
                        Case "ReferencedItem", "ReferencedItem1", "ReferencedItem2", "ReferencedItem3", "ReferencedItem4", "ReferencedItem5"
                            If .ReferenceType = ReferencesType.Item Then
                                If (.ReferenceMatch = "item1" AndAlso (sReference = "ReferencedItem" OrElse sReference = "ReferencedItem1")) _
                                    OrElse (.ReferenceMatch = "item2" AndAlso sReference = "ReferencedItem2") _
                                    OrElse (.ReferenceMatch = "item3" AndAlso sReference = "ReferencedItem3") _
                                    OrElse (.ReferenceMatch = "item4" AndAlso sReference = "ReferencedItem4") _
                                    OrElse (.ReferenceMatch = "item5" AndAlso sReference = "ReferencedItem5") Then
                                    If .Items.Count > 0 AndAlso .Items(0).MatchingPossibilities.Count > 0 Then
                                        Return .Items(0).MatchingPossibilities(0)
                                    Else
                                        Return Nothing
                                    End If
                                End If
                            End If
                        Case "ReferencedNumber", "ReferencedNumber1", "ReferencedNumber2", "ReferencedNumber3", "ReferencedNumber4", "ReferencedNumber5"
                            If .ReferenceType = ReferencesType.Number Then
                                If (.ReferenceMatch = "number1" AndAlso (sReference = "ReferencedNumber" OrElse sReference = "ReferencedNumber1")) _
                                    OrElse (.ReferenceMatch = "number2" AndAlso sReference = "ReferencedNumber2") _
                                    OrElse (.ReferenceMatch = "number3" AndAlso sReference = "ReferencedNumber3") _
                                    OrElse (.ReferenceMatch = "number4" AndAlso sReference = "ReferencedNumber4") _
                                    OrElse (.ReferenceMatch = "number5" AndAlso sReference = "ReferencedNumber5") Then
                                    If .Items.Count > 0 AndAlso .Items(0).MatchingPossibilities.Count > 0 Then
                                        Return .Items(0).MatchingPossibilities(0)
                                    Else
                                        Return Nothing
                                    End If
                                End If
                            End If
                    End Select

                End If
            End With
        Next
        Return Nothing
    End Function

    Private Function PassSingleRestriction(ByVal restx As clsRestriction, Optional ByVal bIgnoreReferences As Boolean = False) As Boolean

        Try
            Dim rest As New clsRestriction
            rest = restx.Copy
            Dim r As Boolean = False
            ' Replace references        
            Select Case rest.sKey1
                Case "ReferencedObject", "ReferencedObject1", "ReferencedObject2", "ReferencedObject3", "ReferencedObject4", "ReferencedObject5", "ReferencedObjects", "ReferencedDirection", "ReferencedCharacter", "ReferencedCharacter1", "ReferencedCharacter2", "ReferencedCharacter3", "ReferencedCharacter4", "ReferencedCharacter5", "ReferencedLocation", "ReferencedItem"
                    rest.sKey1 = GetReference(rest.sKey1)
                    If bIgnoreReferences AndAlso rest.sKey1 Is Nothing Then Return True
            End Select
            Select Case rest.sKey2
                Case "ReferencedObject", "ReferencedObject1", "ReferencedObject2", "ReferencedObject3", "ReferencedObject4", "ReferencedObject5", "ReferencedObjects", "ReferencedDirection", "ReferencedCharacter", "ReferencedCharacter1", "ReferencedCharacter2", "ReferencedCharacter3", "ReferencedCharacter4", "ReferencedCharacter5", "ReferencedLocation", "ReferencedItem"
                    rest.sKey2 = GetReference(rest.sKey2)
                    If bIgnoreReferences AndAlso rest.sKey2 Is Nothing Then Return True
            End Select

            If rest.sKey1 = "%Player%" AndAlso Adventure.Player IsNot Nothing Then rest.sKey1 = Adventure.Player.Key
            If rest.sKey2 = "%Player%" AndAlso Adventure.Player IsNot Nothing Then rest.sKey2 = Adventure.Player.Key

            If Not rest.eType = clsRestriction.RestrictionTypeEnum.Expression AndAlso (rest.sKey1 Is Nothing OrElse (rest.sKey2 Is Nothing AndAlso rest.eType <> clsRestriction.RestrictionTypeEnum.Task AndAlso rest.eType <> clsRestriction.RestrictionTypeEnum.Variable AndAlso rest.eType <> clsRestriction.RestrictionTypeEnum.Direction AndAlso Not (rest.eType = clsRestriction.RestrictionTypeEnum.Object AndAlso rest.eObject = clsRestriction.ObjectEnum.BeHidden))) Then
                r = False
                GoTo SkipTest
            End If

            Select Case rest.eType
                Case clsRestriction.RestrictionTypeEnum.Location
                    Dim loc As clsLocation = Nothing
                    Select Case rest.sKey1
                        Case PLAYERLOCATION
                            loc = Adventure.htblLocations(Adventure.Player.Location.LocationKey)
                        Case Else
                            If Adventure.htblLocations.ContainsKey(rest.sKey1) Then loc = Adventure.htblLocations(rest.sKey1)
                    End Select
                    If loc IsNot Nothing Then
                        Select Case rest.eLocation
                            Case clsRestriction.LocationEnum.HaveBeenSeenByCharacter
                                r = loc.SeenBy(rest.sKey2)
                            Case clsRestriction.LocationEnum.BeInGroup
                                Select Case rest.sKey1
                                    Case PLAYERLOCATION
                                        r = Adventure.htblGroups(rest.sKey2).arlMembers.Contains(Adventure.Player.Location.LocationKey)
                                    Case Else
                                        r = Adventure.htblGroups(rest.sKey2).arlMembers.Contains(rest.sKey1)
                                End Select
                            Case clsRestriction.LocationEnum.HaveProperty
                                r = loc.HasProperty(rest.sKey2)
                            Case clsRestriction.LocationEnum.BeLocation
                                Select Case rest.sKey1
                                    Case PLAYERLOCATION
                                        r = (Adventure.Player.Location.LocationKey = rest.sKey2)
                                    Case Else
                                        r = (rest.sKey1 = rest.sKey2)
                                End Select
                            Case clsRestriction.LocationEnum.Exist
                                r = True
                        End Select
                    End If

                Case clsRestriction.RestrictionTypeEnum.Object
                    Select Case rest.eObject
                        Case clsRestriction.ObjectEnum.BeAtLocation
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    r = Adventure.htblLocations(rest.sKey2).ObjectsInLocation(clsLocation.WhichObjectsToListEnum.AllObjects, False).Count = 0
                                Case ANYOBJECT
                                    r = Adventure.htblLocations(rest.sKey2).ObjectsInLocation(clsLocation.WhichObjectsToListEnum.AllObjects, False).Count > 0
                                Case Else
                                    r = Adventure.htblObjects(rest.sKey1).ExistsAtLocation(rest.sKey2)
                            End Select
                        Case clsRestriction.ObjectEnum.BeHeldByCharacter
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    Select Case rest.sKey2
                                        Case ANYCHARACTER
                                            TODO("No object held by any character test")
                                        Case Else
                                            r = Adventure.htblCharacters(rest.sKey2).HeldObjects.Count = 0
                                    End Select
                                Case ANYOBJECT
                                    Select Case rest.sKey2
                                        Case ANYCHARACTER
                                            TODO("Any object held by any character test")
                                        Case Else
                                            r = Adventure.htblCharacters(rest.sKey2).HeldObjects.Count > 0
                                    End Select
                                Case Else
                                    Select Case rest.sKey2
                                        Case ANYCHARACTER
                                            r = Adventure.htblObjects(rest.sKey1).IsHeldByAnyone
                                        Case Else
                                            r = Adventure.htblCharacters(rest.sKey2).IsHoldingObject(rest.sKey1)
                                    End Select
                            End Select
                        Case clsRestriction.ObjectEnum.BeHidden
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    TODO("No object hidden")
                                Case ANYOBJECT
                                    TODO("Any object hidden")
                                Case Else
                                    With Adventure.htblObjects(rest.sKey1)
                                        If .IsStatic Then
                                            r = .Location.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.NoRooms
                                        Else
                                            r = .Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.Hidden
                                        End If
                                    End With
                            End Select
                        Case clsRestriction.ObjectEnum.BeInGroup
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    r = Adventure.htblGroups(rest.sKey2).arlMembers.Count = 0
                                Case ANYOBJECT
                                    r = Adventure.htblGroups(rest.sKey2).arlMembers.Count > 0
                                Case Else
                                    r = Adventure.htblGroups(rest.sKey2).arlMembers.Contains(rest.sKey1)
                            End Select
                        Case clsRestriction.ObjectEnum.BeInsideObject
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    Select Case rest.sKey2
                                        Case NOOBJECT
                                            TODO("No object in no object test")
                                        Case ANYOBJECT
                                            TODO("No object in any object test")
                                        Case Else
                                            r = Adventure.htblObjects(rest.sKey2).Children(clsObject.WhereChildrenEnum.InsideObject).Count = 0
                                    End Select
                                Case ANYOBJECT
                                    Select Case rest.sKey2
                                        Case NOOBJECT
                                            TODO("Any object in no object test")
                                        Case ANYOBJECT
                                            TODO("Any object in any object test")
                                        Case Else
                                            r = Adventure.htblObjects(rest.sKey2).Children(clsObject.WhereChildrenEnum.InsideObject).Count > 0
                                    End Select
                                Case Else
                                    Select Case rest.sKey2
                                        Case NOOBJECT
                                            TODO("Object inside no object test")
                                        Case ANYOBJECT
                                            r = (Adventure.htblObjects(rest.sKey1).Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject)
                                        Case Else
                                            r = Adventure.htblObjects(rest.sKey1).IsInside(rest.sKey2)
                                    End Select
                            End Select
                        Case clsRestriction.ObjectEnum.BeInState
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    For Each ob As clsObject In Adventure.htblObjects.Values
                                        For Each prop As clsProperty In ob.htblProperties.Values
                                            If prop.Type = clsProperty.PropertyTypeEnum.StateList AndAlso prop.Value = rest.sKey2 AndAlso prop.AppendToProperty = "" Then
                                                r = False
                                                GoTo SkipTest
                                            End If
                                        Next
                                    Next
                                    r = True
                                Case ANYOBJECT
                                    For Each ob As clsObject In Adventure.htblObjects.Values
                                        For Each prop As clsProperty In ob.htblProperties.Values
                                            If prop.Type = clsProperty.PropertyTypeEnum.StateList AndAlso prop.Value = rest.sKey2 AndAlso prop.AppendToProperty = "" Then
                                                r = True
                                                GoTo SkipTest
                                            End If
                                        Next
                                    Next
                                Case Else
                                    ' Ok, this is a fudge option
                                    For Each prop As clsProperty In Adventure.htblObjects(rest.sKey1).htblProperties.Values
                                        If prop.Type = clsProperty.PropertyTypeEnum.StateList AndAlso prop.Value = rest.sKey2 AndAlso prop.AppendToProperty = "" Then
                                            r = True
                                            GoTo SkipTest
                                        End If
                                    Next
                            End Select
                        Case clsRestriction.ObjectEnum.BeOnObject
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    Select Case rest.sKey2
                                        Case NOOBJECT
                                            TODO("No Object must be on No Object test")
                                        Case ANYOBJECT
                                            TODO("No Object must be on Any Object test")
                                        Case Else
                                            r = Adventure.htblObjects(rest.sKey2).Children(clsObject.WhereChildrenEnum.OnObject).Count = 0
                                    End Select
                                Case ANYOBJECT
                                    Select Case rest.sKey2
                                        Case NOOBJECT
                                            TODO("Any Object must be on No Object test")
                                        Case ANYOBJECT
                                            TODO("Any Object must be on Any Object test")
                                        Case Else
                                            r = Adventure.htblObjects(rest.sKey2).Children(clsObject.WhereChildrenEnum.OnObject).Count > 0
                                    End Select
                                Case Else
                                    Select Case rest.sKey2
                                        Case NOOBJECT
                                            TODO("Object must be on No Object test")
                                        Case ANYOBJECT
                                            r = (Adventure.htblObjects(rest.sKey1).Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject)
                                        Case Else
                                            r = Adventure.htblObjects(rest.sKey1).IsOn(rest.sKey2)
                                    End Select
                            End Select
                        Case clsRestriction.ObjectEnum.BePartOfCharacter
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    TODO("No Object is part of character test")
                                Case ANYOBJECT
                                    TODO("Any Object is part of character test")
                                Case Else
                                    Select Case rest.sKey2
                                        Case ANYCHARACTER
                                            TODO("Object is part of any character test")
                                        Case Else
                                            r = Adventure.htblObjects(rest.sKey1).Location.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter _
                                            AndAlso Adventure.htblObjects(rest.sKey1).Location.Key = rest.sKey2
                                    End Select
                            End Select
                        Case clsRestriction.ObjectEnum.BePartOfObject
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    TODO("No Object is part of object test")
                                Case ANYOBJECT
                                    TODO("Any Object is part of object test")
                                Case Else
                                    Select Case rest.sKey2
                                        Case NOOBJECT
                                            TODO("Object is part of No object test")
                                        Case ANYOBJECT
                                            r = Adventure.htblObjects(rest.sKey1).Location.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.PartOfObject
                                        Case Else
                                            r = Adventure.htblObjects(rest.sKey1).Location.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.PartOfObject _
                                             AndAlso Adventure.htblObjects(rest.sKey1).Location.Key = rest.sKey2
                                    End Select
                            End Select
                        Case clsRestriction.ObjectEnum.HaveBeenSeenByCharacter
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    TODO("No Object has been seen by character test")
                                Case ANYOBJECT
                                    TODO("Any Object has been seen by character test")
                                Case Else
                                    Select Case rest.sKey2
                                        Case ANYCHARACTER
                                            TODO("Object has been seen by any character test")
                                        Case Else
                                            r = Adventure.htblCharacters(rest.sKey2).HasSeenObject(rest.sKey1)
                                    End Select
                            End Select
                        Case clsRestriction.ObjectEnum.BeVisibleToCharacter
                            Dim ch As clsCharacter = Adventure.htblCharacters(rest.sKey2)
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    Select Case rest.sKey2
                                        Case ANYCHARACTER
                                            TODO("No Object visible to any character test")
                                        Case Else
                                            r = True
                                            For Each ob As clsObject In Adventure.htblObjects.Values
                                                If ch.CanSeeObject(ob.Key) Then
                                                    r = False
                                                    Exit For
                                                End If
                                            Next
                                    End Select
                                Case ANYOBJECT
                                    Select Case rest.sKey2
                                        Case ANYCHARACTER
                                            TODO("Any Object visible to any character test")
                                        Case Else
                                            r = False
                                            For Each ob As clsObject In Adventure.htblObjects.Values
                                                If ch.CanSeeObject(ob.Key) Then
                                                    r = True
                                                    Exit For
                                                End If
                                            Next
                                    End Select
                                Case Else
                                    Select Case rest.sKey2
                                        Case ANYCHARACTER
                                            TODO("Object visible to any character test")
                                        Case Else
                                            r = ch.CanSeeObject(rest.sKey1)
                                    End Select
                            End Select
                        Case clsRestriction.ObjectEnum.BeWornByCharacter
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    Select Case rest.sKey2
                                        Case ANYCHARACTER
                                            TODO("No Object worn by any character test")
                                        Case Else
                                            r = Adventure.htblCharacters(rest.sKey2).WornObjects.Count = 0
                                    End Select
                                Case ANYOBJECT
                                    Select Case rest.sKey2
                                        Case ANYCHARACTER
                                            TODO("Any Object worn by any character test")
                                        Case Else
                                            r = Adventure.htblCharacters(rest.sKey2).WornObjects.Count > 0
                                    End Select
                                Case Else
                                    If Adventure.htblObjects.ContainsKey(rest.sKey1) Then
                                        Select Case rest.sKey2
                                            Case ANYCHARACTER
                                                r = Adventure.htblObjects(rest.sKey1).Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                                            Case Else
                                                r = Adventure.htblCharacters(rest.sKey2).IsWearingObject(rest.sKey1)
                                        End Select
                                    End If
                            End Select

                        Case clsRestriction.ObjectEnum.Exist
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    r = Adventure.htblObjects.Count = 0
                                Case ANYOBJECT
                                    r = Adventure.htblObjects.Count > 0
                                Case Else
                                    r = Adventure.htblObjects.ContainsKey(rest.sKey1)
                            End Select
                        Case clsRestriction.ObjectEnum.HaveProperty
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    For Each ob As clsObject In Adventure.htblObjects.Values
                                        If ob.HasProperty(rest.sKey2) Then
                                            r = False
                                            GoTo SkipTest
                                        End If
                                    Next
                                    r = True
                                Case ANYOBJECT
                                    For Each ob As clsObject In Adventure.htblObjects.Values
                                        If ob.HasProperty(rest.sKey2) Then
                                            r = True
                                            GoTo SkipTest
                                        End If
                                    Next
                                Case Else
                                    r = Adventure.htblObjects.ContainsKey(rest.sKey1) AndAlso Adventure.htblObjects(rest.sKey1).HasProperty(rest.sKey2)
                            End Select
                        Case clsRestriction.ObjectEnum.BeExactText
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    TODO("No Object be exact text test")
                                Case ANYOBJECT
                                    TODO("Any Object be exact text test")
                                Case Else
                                    r = restx.sKey1 = "ReferencedObjects" AndAlso NewReferences IsNot Nothing AndAlso (NewReferences(0).Items(0).sCommandReference = "all" OrElse (NewReferences.Length > 1 AndAlso NewReferences(1).Items(0).sCommandReference = "all")) ' Should probably verify that %objects% was actually reference 0
                            End Select
                        Case clsRestriction.ObjectEnum.BeObject
                            Select Case rest.sKey1
                                Case NOOBJECT
                                    TODO("No Object be specific object test")
                                Case ANYOBJECT
                                    TODO("Any Object be specific object test")
                                Case Else
                                    r = (rest.sKey1 = rest.sKey2)
                            End Select
                    End Select

                Case clsRestriction.RestrictionTypeEnum.Task
                    Select Case rest.eTask
                        Case clsRestriction.TaskEnum.Complete
                            If Adventure.htblTasks.ContainsKey(rest.sKey1) Then r = Adventure.htblTasks(rest.sKey1).Completed
                    End Select

                Case clsRestriction.RestrictionTypeEnum.Variable
                    Dim var As clsVariable
                    Dim iIndex As Integer = 1
                    If rest.sKey1.StartsWith("ReferencedNumber") Then
                        Dim iRef As Integer = Math.Max(SafeInt(rest.sKey1.Replace("ReferencedNumber", "")) - 1, 0)
                        var = New clsVariable
                        var.Type = clsVariable.VariableTypeEnum.Numeric
                        var.IntValue = SafeInt(GetReference(rest.sKey1))
                    ElseIf rest.sKey1.StartsWith("ReferencedText") Then
                        Dim iRef As Integer = Math.Max(SafeInt(rest.sKey1.Replace("ReferencedText", "")) - 1, 0)
                        var = New clsVariable
                        var.Type = clsVariable.VariableTypeEnum.Text
                        var.StringValue = Adventure.sReferencedText(iRef)
                    Else
                        var = Adventure.htblVariables(rest.sKey1)
                        If rest.sKey2 <> "" Then
                            If Adventure.htblVariables.ContainsKey(rest.sKey2) Then
                                iIndex = Adventure.htblVariables(rest.sKey2).IntValue
                            Else
                                iIndex = SafeInt(EvaluateExpression(rest.sKey2))
                            End If
                        End If
                    End If

                    Dim iIntVal As Integer
                    Dim sStringVal As String = ""
                    If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                        If rest.IntValue = Integer.MinValue AndAlso rest.StringValue <> "" Then
                            iIntVal = Adventure.htblVariables(rest.StringValue).IntValue ' Variable value
                        Else
                            If rest.StringValue <> "" AndAlso rest.StringValue <> rest.IntValue.ToString Then
                                iIntVal = SafeInt(EvaluateExpression(rest.StringValue)) ' Expression
                            Else
                                iIntVal = rest.IntValue ' Integer value
                            End If
                        End If
                    Else
                        If rest.IntValue = Integer.MinValue AndAlso rest.StringValue <> "" Then
                            sStringVal = Adventure.htblVariables(rest.StringValue).StringValue
                        Else
                            sStringVal = EvaluateExpression(rest.StringValue)
                        End If
                    End If

                    Select Case rest.eVariable
                        Case clsRestriction.VariableEnum.EqualTo
                            If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                                r = (var.IntValue(iIndex) = iIntVal)
                            Else
                                r = (var.StringValue(iIndex) = sStringVal)
                            End If
                        Case clsRestriction.VariableEnum.GreaterThan
                            If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                                r = (var.IntValue(iIndex) > iIntVal)
                            Else
                                r = (var.StringValue(iIndex) > sStringVal)
                            End If
                        Case clsRestriction.VariableEnum.GreaterThanOrEqualTo
                            If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                                r = (var.IntValue(iIndex) >= iIntVal)
                            Else
                                r = (var.StringValue(iIndex) >= sStringVal)
                            End If
                        Case clsRestriction.VariableEnum.LessThan
                            If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                                r = (var.IntValue(iIndex) < iIntVal)
                            Else
                                r = (var.StringValue(iIndex) < sStringVal)
                            End If
                        Case clsRestriction.VariableEnum.LessThanOrEqualTo
                            If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                                r = (var.IntValue(iIndex) <= iIntVal)
                            Else
                                r = (var.StringValue(iIndex) <= sStringVal)
                            End If
                        Case clsRestriction.VariableEnum.Contain
                            If var.Type = clsVariable.VariableTypeEnum.Text Then
                                r = var.StringValue(iIndex) IsNot Nothing AndAlso var.StringValue(iIndex).Contains(sStringVal)
                            End If
                    End Select

                Case clsRestriction.RestrictionTypeEnum.Character
                    Select Case rest.eCharacter
                        Case clsRestriction.CharacterEnum.BeAlone
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    For Each ch As clsCharacter In Adventure.htblCharacters.Values
                                        If ch.IsAlone Then
                                            r = True
                                            Exit For
                                        End If
                                    Next
                                    r = False
                                Case Else
                                    r = Adventure.htblCharacters(rest.sKey1).IsAlone
                            End Select

                        Case clsRestriction.CharacterEnum.BeAloneWith
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    TODO("Any Character be alone")
                                Case Else
                                    If rest.sKey2 = ANYCHARACTER Then
                                        r = Adventure.htblCharacters(rest.sKey1).AloneWithChar IsNot Nothing
                                    Else
                                        r = Adventure.htblCharacters(rest.sKey1).AloneWithChar = rest.sKey2
                                    End If
                            End Select

                        Case clsRestriction.CharacterEnum.BeAtLocation
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    r = False
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        If c.Location.LocationKey = rest.sKey2 Then
                                            r = True
                                            Exit For
                                        End If
                                    Next
                                Case Else
                                    r = Adventure.htblCharacters(rest.sKey1).Location.LocationKey = rest.sKey2
                            End Select

                        Case clsRestriction.CharacterEnum.BeCharacter
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    r = True ' Bit pointless
                                Case Else
                                    r = (rest.sKey1 = rest.sKey2)
                            End Select

                        Case clsRestriction.CharacterEnum.BeInConversationWith
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    TODO("Any Character be in conversation with")
                                Case Else
                                    If rest.sKey2 = ANYCHARACTER Then
                                        r = (Adventure.sConversationCharKey <> "")
                                    Else
                                        r = (Adventure.sConversationCharKey = rest.sKey2)
                                    End If
                            End Select

                        Case clsRestriction.CharacterEnum.Exist
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    r = Adventure.htblCharacters.Count > 0
                                Case Else
                                    r = Adventure.htblCharacters.ContainsKey(rest.sKey1)
                            End Select

                        Case clsRestriction.CharacterEnum.HaveRouteInDirection
                            sRouteError = rest.oMessage.ToString
                            Dim sSpecificError As String = ""
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    r = False
                                    Select Case rest.sKey2
                                        Case ANYDIRECTION
                                            For Each c As clsCharacter In Adventure.htblCharacters.Values
                                                For Each d As DirectionsEnum In [Enum].GetValues(GetType(DirectionsEnum))
                                                    If c.HasRouteInDirection(d, False, , sSpecificError) Then
                                                        r = True
                                                        GoTo DoneAnyCharAnyDir
                                                    End If
                                                Next
                                            Next
DoneAnyCharAnyDir:
                                        Case Else
                                            For Each c As clsCharacter In Adventure.htblCharacters.Values
                                                If c.HasRouteInDirection(CType([Enum].Parse(GetType(DirectionsEnum), rest.sKey2), DirectionsEnum), False, , sSpecificError) Then
                                                    r = True
                                                    Exit For
                                                End If
                                            Next
                                    End Select
                                Case Else
                                    Dim ch As clsCharacter = Adventure.htblCharacters(rest.sKey1)
                                    Select Case rest.sKey2
                                        Case ANYDIRECTION
                                            r = False
                                            For Each d As DirectionsEnum In [Enum].GetValues(GetType(DirectionsEnum))
                                                If ch.HasRouteInDirection(d, False, , sSpecificError) Then
                                                    r = True
                                                    Exit For
                                                End If
                                            Next
                                        Case Else
                                            r = ch.HasRouteInDirection(CType([Enum].Parse(GetType(DirectionsEnum), rest.sKey2), DirectionsEnum), False, , sSpecificError)
                                    End Select
                            End Select
                            If sSpecificError <> "" Then sRouteError = sSpecificError

                        Case clsRestriction.CharacterEnum.HaveSeenCharacter
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    TODO("Any Character seen character")
                                Case Else
                                    r = Adventure.htblCharacters(rest.sKey1).HasSeenCharacter(rest.sKey2)
                            End Select

                        Case clsRestriction.CharacterEnum.HaveSeenLocation
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    TODO("Any Character seen location")
                                Case Else
                                    r = Adventure.htblCharacters(rest.sKey1).HasSeenLocation(rest.sKey2)
                            End Select

                        Case clsRestriction.CharacterEnum.HaveSeenObject
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    TODO("Any Character seen object")
                                Case Else
                                    r = Adventure.htblCharacters(rest.sKey1).HasSeenObject(rest.sKey2)
                            End Select

                        Case clsRestriction.CharacterEnum.BeHoldingObject
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    r = Adventure.htblObjects(rest.sKey2).IsHeldByAnyone
                                Case Else
                                    r = Adventure.htblCharacters(rest.sKey1).IsHoldingObject(rest.sKey2)
                            End Select

                        Case clsRestriction.CharacterEnum.BeInSameLocationAsCharacter
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    r = Not Adventure.htblCharacters(rest.sKey2).IsAlone
                                Case Else
                                    Dim ch As clsCharacter = Adventure.htblCharacters(rest.sKey1)
                                    If rest.sKey2 = ANYCHARACTER Then
                                        r = False
                                        For Each c As clsCharacter In Adventure.htblCharacters.Values
                                            If c.Key <> rest.sKey1 AndAlso c.Location.LocationKey = ch.Location.LocationKey Then
                                                r = True
                                                Exit For
                                            End If
                                        Next
                                    Else
                                        r = ch.Location.LocationKey = Adventure.htblCharacters(rest.sKey2).Location.LocationKey
                                    End If
                            End Select

                        Case clsRestriction.CharacterEnum.BeVisibleToCharacter
                            Dim lChars As New List(Of clsCharacter)

                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        lChars.Add(c)
                                    Next
                                Case Else
                                    lChars.Add(Adventure.htblCharacters(rest.sKey1))
                            End Select

                            r = False
                            For Each c As clsCharacter In lChars
                                If rest.sKey2 = ANYCHARACTER Then
                                    For Each c2 As clsCharacter In Adventure.htblCharacters.Values
                                        If c.Key <> c2.Key Then
                                            If c.CanSeeCharacter(c2.Key) Then
                                                r = True
                                                Exit For
                                            End If
                                        End If
                                    Next
                                Else
                                    If c.CanSeeCharacter(rest.sKey2) Then
                                        r = True
                                        Exit For
                                    End If
                                End If
                            Next

                        Case clsRestriction.CharacterEnum.BeInSameLocationAsObject
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        If c.CanSeeObject(rest.sKey2) Then
                                            r = True
                                            Exit For
                                        End If
                                    Next
                                Case Else
                                    r = Adventure.htblCharacters(rest.sKey1).CanSeeObject(rest.sKey2)
                            End Select

                        Case clsRestriction.CharacterEnum.BeLyingOnObject
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        If c.Location.Position = clsCharacterLocation.PositionEnum.Lying Then
                                            If rest.sKey2 = ANYOBJECT Then
                                                If c.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject Then
                                                    r = True
                                                    Exit For
                                                End If
                                            ElseIf rest.sKey2 = THEFLOOR Then
                                                If c.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation Then
                                                    r = True
                                                    Exit For
                                                End If
                                            Else
                                                If c.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject AndAlso c.Location.Key = rest.sKey2 Then
                                                    r = True
                                                    Exit For
                                                End If
                                            End If
                                        End If
                                    Next
                                Case Else
                                    Dim ch As clsCharacter = Adventure.htblCharacters(rest.sKey1)
                                    If rest.sKey2 = ANYOBJECT Then
                                        r = ch.Location.Position = clsCharacterLocation.PositionEnum.Lying AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject
                                    ElseIf rest.sKey2 = THEFLOOR Then
                                        r = ch.Location.Position = clsCharacterLocation.PositionEnum.Lying AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                    Else
                                        r = (ch.Location.Position = clsCharacterLocation.PositionEnum.Lying AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject AndAlso ch.Location.Key = rest.sKey2)
                                    End If
                            End Select

                        Case clsRestriction.CharacterEnum.BeInGroup
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        If Adventure.htblGroups(rest.sKey2).arlMembers.Contains(c.Key) Then
                                            r = True
                                            Exit For
                                        End If
                                    Next
                                Case Else
                                    r = Adventure.htblGroups(rest.sKey2).arlMembers.Contains(rest.sKey1)
                            End Select

                        Case clsRestriction.CharacterEnum.BeOfGender
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    TODO("Any Character be of gender")
                                Case Else
                                    r = Adventure.htblCharacters(rest.sKey1).Gender = CType([Enum].Parse(GetType(clsCharacter.GenderEnum), rest.sKey2), clsCharacter.GenderEnum)
                            End Select

                        Case clsRestriction.CharacterEnum.BeSittingOnObject
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        If c.Location.Position = clsCharacterLocation.PositionEnum.Sitting Then
                                            If rest.sKey2 = ANYOBJECT Then
                                                If c.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject Then
                                                    r = True
                                                    Exit For
                                                End If
                                            ElseIf rest.sKey2 = THEFLOOR Then
                                                If c.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation Then
                                                    r = True
                                                    Exit For
                                                End If
                                            Else
                                                If c.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject AndAlso c.Location.Key = rest.sKey2 Then
                                                    r = True
                                                    Exit For
                                                End If
                                            End If
                                        End If
                                    Next
                                Case Else
                                    Dim ch As clsCharacter = Adventure.htblCharacters(rest.sKey1)
                                    If rest.sKey2 = ANYOBJECT Then
                                        r = ch.Location.Position = clsCharacterLocation.PositionEnum.Sitting AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject
                                    ElseIf rest.sKey2 = THEFLOOR Then
                                        r = ch.Location.Position = clsCharacterLocation.PositionEnum.Sitting AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                    Else
                                        r = (ch.Location.Position = clsCharacterLocation.PositionEnum.Sitting AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject AndAlso ch.Location.Key = rest.sKey2)
                                    End If
                            End Select

                        Case clsRestriction.CharacterEnum.BeStandingOnObject
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        If c.Location.Position = clsCharacterLocation.PositionEnum.Standing Then
                                            If rest.sKey2 = ANYOBJECT Then
                                                If c.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject Then
                                                    r = True
                                                    Exit For
                                                End If
                                            ElseIf rest.sKey2 = THEFLOOR Then
                                                If c.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation Then
                                                    r = True
                                                    Exit For
                                                End If
                                            Else
                                                If c.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject AndAlso c.Location.Key = rest.sKey2 Then
                                                    r = True
                                                    Exit For
                                                End If
                                            End If
                                        End If
                                    Next
                                Case Else
                                    Dim ch As clsCharacter = Adventure.htblCharacters(rest.sKey1)
                                    If rest.sKey2 = ANYOBJECT Then
                                        r = ch.Location.Position = clsCharacterLocation.PositionEnum.Standing AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject
                                    ElseIf rest.sKey2 = THEFLOOR Then
                                        r = ch.Location.Position = clsCharacterLocation.PositionEnum.Standing AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                                    Else
                                        r = (ch.Location.Position = clsCharacterLocation.PositionEnum.Standing AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject AndAlso ch.Location.Key = rest.sKey2)
                                    End If
                            End Select

                        Case clsRestriction.CharacterEnum.BeWearingObject
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    r = Adventure.htblObjects(rest.sKey2).IsWornByAnyone
                                Case Else
                                    r = Adventure.htblCharacters(rest.sKey1).IsWearingObject(rest.sKey2)
                            End Select

                        Case clsRestriction.CharacterEnum.BeWithinLocationGroup
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        If Adventure.htblGroups(rest.sKey2).arlMembers.Contains(c.Location.LocationKey) Then
                                            r = True
                                            Exit For
                                        End If
                                    Next
                                Case Else
                                    r = Adventure.htblGroups(rest.sKey2).arlMembers.Contains(Adventure.htblCharacters(rest.sKey1).Location.LocationKey)
                            End Select

                        Case clsRestriction.CharacterEnum.HaveProperty
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        If c.HasProperty(rest.sKey2) Then
                                            r = True
                                            Exit For
                                        End If
                                    Next
                                Case Else
                                    r = Adventure.htblCharacters.ContainsKey(rest.sKey1) AndAlso Adventure.htblCharacters(rest.sKey1).HasProperty(rest.sKey2)
                            End Select

                        Case clsRestriction.CharacterEnum.BeInPosition
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    For Each c As clsCharacter In Adventure.htblCharacters.Values
                                        If c.HasProperty("CharacterPosition") AndAlso c.GetPropertyValue("CharacterPosition") = rest.sKey2 Then
                                            r = True
                                            Exit For
                                        End If
                                    Next
                                Case Else
                                    If Adventure.htblCharacters(rest.sKey1).HasProperty("CharacterPosition") Then
                                        r = Adventure.htblCharacters(rest.sKey1).GetPropertyValue("CharacterPosition") = rest.sKey2
                                    End If
                            End Select

                        Case clsRestriction.CharacterEnum.BeInsideObject
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    Select Case rest.sKey2
                                        Case ANYOBJECT
                                            TODO("Any Character be inside any object")
                                        Case Else
                                            r = Adventure.htblObjects(rest.sKey2).ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).Count > 0
                                    End Select
                                Case Else
                                    If rest.sKey2 = ANYOBJECT Then
                                        r = (Adventure.htblCharacters(rest.sKey1).Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.InObject)
                                    Else
                                        If rest.sKey1 = ANYOBJECT Then
                                            r = Adventure.htblObjects(rest.sKey2).Children(clsObject.WhereChildrenEnum.InsideObject).Count > 0
                                        ElseIf rest.sKey1 = ANYCHARACTER Then
                                            r = Adventure.htblObjects(rest.sKey2).ChildrenCharacters(clsObject.WhereChildrenEnum.InsideObject).Count > 0
                                        Else
                                            r = Adventure.htblCharacters(rest.sKey1).Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.InObject AndAlso Adventure.htblCharacters(rest.sKey1).Location.Key = rest.sKey2
                                        End If
                                    End If
                            End Select

                        Case clsRestriction.CharacterEnum.BeOnObject
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    If rest.sKey2 = ANYOBJECT Then
                                        TODO("Any Character on Any Object")
                                    Else
                                        r = Adventure.htblObjects(rest.sKey2).ChildrenCharacters(clsObject.WhereChildrenEnum.OnObject).Count > 0
                                    End If
                                Case Else
                                    If rest.sKey2 = ANYOBJECT Then
                                        r = (Adventure.htblCharacters(rest.sKey1).Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject)
                                    Else
                                        Dim ch As clsCharacter = Adventure.htblCharacters(rest.sKey1)
                                        r = ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject AndAlso ch.Location.Key = rest.sKey2 ' Adventure.htblObjects(rest.sKey1).IsOn(rest.sKey2)                                        
                                    End If
                            End Select

                        Case clsRestriction.CharacterEnum.BeOnCharacter
                            Select Case rest.sKey1
                                Case ANYCHARACTER
                                    TODO("Any Character be on character")
                                Case Else
                                    Dim ch As clsCharacter = Adventure.htblCharacters(rest.sKey1)
                                    If rest.sKey2 = ANYCHARACTER Then
                                        r = (ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnCharacter)
                                    Else
                                        r = ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnCharacter AndAlso ch.Location.Key = rest.sKey2
                                    End If
                            End Select

                    End Select

                Case clsRestriction.RestrictionTypeEnum.Item

                    Dim obItem As clsObject = Nothing
                    Dim chItem As clsCharacter = Nothing
                    Dim locItem As clsLocation = Nothing

                    If Not Adventure.htblObjects.TryGetValue(rest.sKey1, obItem) AndAlso Not Adventure.htblCharacters.TryGetValue(rest.sKey1, chItem) AndAlso Not Adventure.htblLocations.TryGetValue(rest.sKey1, locItem) Then
                        r = False
                        GoTo SkipTest
                    End If

                    Select Case rest.eItem
                        Case clsRestriction.ItemEnum.BeAtLocation
                            If obItem IsNot Nothing Then
                                r = obItem.IsVisibleAtLocation(rest.sKey2)
                            ElseIf chItem IsNot Nothing Then
                                r = chItem.Location.LocationKey = rest.sKey2
                            Else
                                r = locItem.Key = rest.sKey2
                            End If
                        Case clsRestriction.ItemEnum.BeCharacter
                            r = chItem IsNot Nothing AndAlso chItem.Key = rest.sKey2
                        Case clsRestriction.ItemEnum.BeInGroup
                            Dim grp As clsGroup = Nothing
                            If Adventure.htblGroups.TryGetValue(rest.sKey2, grp) Then
                                r = grp.arlMembers.Contains(rest.sKey1)
                            End If
                        Case clsRestriction.ItemEnum.BeInSameLocationAsCharacter
                            Dim chKey2 As clsCharacter = Nothing
                            If Adventure.htblCharacters.TryGetValue(rest.sKey2, chKey2) Then
                                If obItem IsNot Nothing Then
                                    r = chKey2.CanSeeObject(obItem.Key)
                                ElseIf chItem IsNot Nothing Then
                                    r = chKey2.CanSeeCharacter(chItem.Key)
                                Else
                                    r = chKey2.Location.LocationKey = locItem.Key
                                End If
                            End If
                        Case clsRestriction.ItemEnum.BeInSameLocationAsObject
                            Dim obKey2 As clsObject = Nothing
                            If Adventure.htblObjects.TryGetValue(rest.sKey2, obKey2) Then
                                r = False
                                If obItem IsNot Nothing Then
                                    For Each sOb1Loc As String In obItem.LocationRoots.Keys
                                        For Each sOb2Loc As String In obKey2.LocationRoots.Keys
                                            If sOb1Loc = sOb2Loc Then r = True
                                            Exit For
                                        Next
                                    Next
                                ElseIf chItem IsNot Nothing Then
                                    r = chItem.CanSeeObject(obKey2.Key)
                                Else
                                    r = obKey2.LocationRoots.ContainsKey(locItem.Key)
                                End If
                            End If
                        Case clsRestriction.ItemEnum.BeInsideObject
                            Dim obKey2 As clsObject = Nothing
                            If Adventure.htblObjects.TryGetValue(rest.sKey2, obKey2) Then
                                r = obKey2.Children(clsObject.WhereChildrenEnum.InsideObject, True).ContainsKey(rest.sKey1)
                            End If
                        Case clsRestriction.ItemEnum.BeLocation
                            r = locItem IsNot Nothing AndAlso locItem.Key = rest.sKey2
                        Case clsRestriction.ItemEnum.BeObject
                            r = obItem IsNot Nothing AndAlso obItem.Key = rest.sKey2
                        Case clsRestriction.ItemEnum.BeOnCharacter
                            Dim chKey2 As clsCharacter = Nothing
                            If Adventure.htblCharacters.TryGetValue(rest.sKey2, chKey2) Then
                                If obItem IsNot Nothing Then
                                    r = obItem.Location.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter AndAlso obItem.Location.Key = chKey2.Key
                                ElseIf chItem IsNot Nothing Then
                                    r = chKey2.Children(True).ContainsKey(chItem.Key)
                                Else
                                    r = False
                                End If
                            End If
                        Case clsRestriction.ItemEnum.BeType
                            r = (rest.sKey2 = "Object" AndAlso obItem IsNot Nothing) OrElse (rest.sKey2 = "Character" AndAlso chItem IsNot Nothing) OrElse (rest.sKey2 = "Location" AndAlso locItem IsNot Nothing)
                        Case clsRestriction.ItemEnum.Exist
                            r = obItem IsNot Nothing OrElse chItem IsNot Nothing OrElse locItem IsNot Nothing
                        Case clsRestriction.ItemEnum.HaveProperty
                            If obItem IsNot Nothing Then
                                r = obItem.HasProperty(rest.sKey2)
                            ElseIf chItem IsNot Nothing Then
                                r = chItem.HasProperty(rest.sKey2)
                            Else
                                r = locItem.HasProperty(rest.sKey2)
                            End If
                    End Select

                Case clsRestriction.RestrictionTypeEnum.Property
                    Dim item As clsItem = Nothing
                    Dim bItemContainsProperty As Boolean = False
                    If Adventure.htblObjects.ContainsKey(rest.sKey2) Then
                        item = Adventure.htblObjects(rest.sKey2)
                        bItemContainsProperty = CType(item, clsObject).HasProperty(rest.sKey1)
                    ElseIf Adventure.htblCharacters.ContainsKey(rest.sKey2) Then
                        item = Adventure.htblCharacters(rest.sKey2)
                        bItemContainsProperty = CType(item, clsCharacter).HasProperty(rest.sKey1)
                    ElseIf Adventure.htblLocations.ContainsKey(rest.sKey2) Then
                        item = Adventure.htblLocations(rest.sKey2)
                        bItemContainsProperty = CType(item, clsLocation).HasProperty(rest.sKey1)
                    End If

                    If Not bItemContainsProperty Then
                        r = False
                    Else
                        Dim prop As clsProperty
                        If TypeOf item Is clsObject Then
                            prop = CType(item, clsObject).GetProperty(rest.sKey1)
                        ElseIf TypeOf item Is clsCharacter Then
                            prop = CType(item, clsCharacter).GetProperty(rest.sKey1)
                        Else
                            prop = CType(item, clsLocation).GetProperty(rest.sKey1)
                        End If

                        Select Case prop.Type
                            Case clsProperty.PropertyTypeEnum.CharacterKey, clsProperty.PropertyTypeEnum.LocationGroupKey, clsProperty.PropertyTypeEnum.LocationKey, clsProperty.PropertyTypeEnum.ObjectKey
                                Dim sKey As String = GetReference(rest.StringValue)
                                If sKey Is Nothing Then sKey = rest.StringValue
                                r = (prop.Value = sKey)
                            Case clsProperty.PropertyTypeEnum.Integer
                                Dim iCompareValue As Integer = 0
                                If Adventure.htblVariables.ContainsKey(rest.StringValue) Then
                                    iCompareValue = Adventure.htblVariables(rest.StringValue).IntValue
                                Else
                                    If bIgnoreReferences Then
                                        For Each sRef As String In ReferenceNames()
                                            If rest.StringValue.Contains(sRef) Then
                                                r = True
                                                GoTo SkipTest
                                            End If
                                        Next
                                    End If
                                    iCompareValue = EvaluateExpression(rest.StringValue, True)
                                End If
                                Select Case CType(rest.IntValue, clsRestriction.VariableEnum)
                                    Case clsRestriction.VariableEnum.LessThan
                                        r = (SafeInt(prop.Value) < iCompareValue)
                                    Case clsRestriction.VariableEnum.LessThanOrEqualTo
                                        r = (SafeInt(prop.Value) <= iCompareValue)
                                    Case clsRestriction.VariableEnum.EqualTo, CType(-1, clsRestriction.VariableEnum)
                                        r = (SafeInt(prop.Value) = iCompareValue)
                                    Case clsRestriction.VariableEnum.GreaterThanOrEqualTo
                                        r = (SafeInt(prop.Value) >= iCompareValue)
                                    Case clsRestriction.VariableEnum.GreaterThan
                                        r = (SafeInt(prop.Value) > iCompareValue)
                                End Select
                            Case clsProperty.PropertyTypeEnum.SelectionOnly
                                r = True
                            Case clsProperty.PropertyTypeEnum.StateList
                                r = prop.Value = rest.StringValue
                            Case clsProperty.PropertyTypeEnum.Text
                                Dim sStringVal As String
                                ' Could be an expression or simple text
                                If rest.StringValue.Contains("""") Then
                                    sStringVal = EvaluateExpression(rest.StringValue)
                                Else
                                    sStringVal = ReplaceFunctions(rest.StringValue)
                                End If
                                r = (prop.Value = sStringVal)
                            Case clsProperty.PropertyTypeEnum.ValueList
                                Dim iCompareValue As Integer = 0
                                If IsNumeric(rest.StringValue) Then
                                    iCompareValue = SafeInt(rest.StringValue)
                                Else
                                    If bIgnoreReferences Then
                                        For Each sRef As String In ReferenceNames()
                                            If rest.StringValue.Contains(sRef) Then
                                                r = True
                                                GoTo SkipTest
                                            End If
                                        Next
                                    End If
                                    iCompareValue = EvaluateExpression(rest.StringValue, True)
                                End If
                                Select Case CType(rest.IntValue, clsRestriction.VariableEnum)
                                    Case clsRestriction.VariableEnum.LessThan
                                        r = (SafeInt(prop.Value) < iCompareValue)
                                    Case clsRestriction.VariableEnum.LessThanOrEqualTo
                                        r = (SafeInt(prop.Value) <= iCompareValue)
                                    Case clsRestriction.VariableEnum.EqualTo, CType(-1, clsRestriction.VariableEnum)
                                        r = (SafeInt(prop.Value) = iCompareValue)
                                    Case clsRestriction.VariableEnum.GreaterThanOrEqualTo
                                        r = (SafeInt(prop.Value) >= iCompareValue)
                                    Case clsRestriction.VariableEnum.GreaterThan
                                        r = (SafeInt(prop.Value) > iCompareValue)
                                End Select
                        End Select
                    End If

                Case clsRestriction.RestrictionTypeEnum.Direction
                    Dim sRefDirection As String = GetReference("ReferencedDirection")
                    r = rest.sKey1 = sRefDirection

                Case clsRestriction.RestrictionTypeEnum.Expression
                    r = SafeBool(EvaluateExpression(rest.StringValue))

            End Select

SkipTest:
            If r = (rest.eMust = clsRestriction.MustEnum.Must) Then
                sRestrictionText = ""
                DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, restx.Summary & ": Passed")
                Return True
            Else
                sRestrictionText = restx.oMessage.ToString ' Use original, so we mark any Display Once as seen
                If rest.eType = clsRestriction.RestrictionTypeEnum.Character AndAlso rest.eCharacter = clsRestriction.CharacterEnum.HaveRouteInDirection Then
                    If sRouteError <> sRestrictionText AndAlso sRouteError <> "" Then sRestrictionText = sRouteError
                End If
                DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, restx.Summary & ": Failed")
            End If

        Catch ex As Exception
            ErrMsg("Error evaluating PassSingleRestriction for restriction """ & restx.ToString & """", ex)
        End Try

    End Function


    Private Function EvaluateRestrictionBlock(ByVal arlRestrictions As RestrictionArrayList, ByVal sBlock As String, Optional ByVal bIgnoreReferences As Boolean = False, Optional tas As clsTask = Nothing) As Boolean

        While sBlock.Contains("A#O")
            ' #A#O# => (#A#)O#
            ' #A#A#O# => (#A#A#)O#
            '(#O#)A#O# => ((#O#)A#)O#
            '#A(#A#O#) => #A((#A#)O#)
            ' Insert a ( at beginning, or after previous (
            Dim i As Integer = sBlock.IndexOf("A#O")
            Dim j As Integer = sBlock.Substring(0, i).LastIndexOf("(") + 1
            sBlock = sLeft(sBlock, j) & "(" & sBlock.Substring(j, i - j) & "A#)O" & sBlock.Substring(i + 3)
        End While

        Select Case Left(sBlock, 1)
            Case "("
                ' Get block
                Dim iBrackDepth As Integer = 1
                Dim sSubBlock As String = "("
                Dim iOffset As Integer = 1
                While iBrackDepth > 0
                    Dim s As String = sBlock.Substring(iOffset, 1)
                    Select Case s
                        Case "("
                            iBrackDepth += 1
                        Case ")"
                            iBrackDepth -= 1
                        Case Else
                            ' Do nothing
                    End Select
                    sSubBlock &= s
                    iOffset += 1
                End While
                sSubBlock = sSubBlock.Substring(1, sSubBlock.Length - 2)
                If sBlock.Length - 2 = sSubBlock.Length Then
                    Return EvaluateRestrictionBlock(arlRestrictions, sSubBlock, bIgnoreReferences)
                Else
                    Select Case sBlock.Substring(sSubBlock.Length + 2, 1)
                        Case "A"
                            Dim bFirst As Boolean = EvaluateRestrictionBlock(arlRestrictions, sSubBlock, bIgnoreReferences)
                            If Not bFirst Then
                                iRestNum += CharacterCount(sBlock.Substring(sSubBlock.Length + 3), "#"c)
                                Return False
                            Else
                                Return EvaluateRestrictionBlock(arlRestrictions, sBlock.Substring(sSubBlock.Length + 3), bIgnoreReferences)
                            End If
                        Case "O"
                            Dim bFirst As Boolean = EvaluateRestrictionBlock(arlRestrictions, sSubBlock, bIgnoreReferences)
                            If bFirst Then
                                iRestNum += CharacterCount(sBlock.Substring(sSubBlock.Length + 3), "#"c)
                                Return True
                            Else
                                Return EvaluateRestrictionBlock(arlRestrictions, sBlock.Substring(sSubBlock.Length + 3), bIgnoreReferences)
                            End If
                        Case Else
                            ' Error
                    End Select
                End If
            Case "#"
                iRestNum += 1
                If sBlock.Length = 1 Then
                    Return PassSingleRestriction(arlRestrictions(iRestNum - 1), bIgnoreReferences)
                Else
                    Select Case sBlock.Substring(1, 1)
                        Case "A"
                            Dim bFirst As Boolean = PassSingleRestriction(arlRestrictions(iRestNum - 1), bIgnoreReferences)
                            If Not bFirst Then
                                iRestNum += CharacterCount(sBlock.Substring(2), "#"c)
                                Return False
                            Else
                                Return EvaluateRestrictionBlock(arlRestrictions, sBlock.Substring(2), bIgnoreReferences)
                            End If
                        Case "O"
                            Dim bFirst As Boolean = PassSingleRestriction(arlRestrictions(iRestNum - 1), bIgnoreReferences)
                            If bFirst Then
                                iRestNum += CharacterCount(sBlock.Substring(2), "#"c)
                                Return True
                            Else
                                Return EvaluateRestrictionBlock(arlRestrictions, sBlock.Substring(2), bIgnoreReferences)
                            End If
                        Case Else
                            ' Error
                    End Select
                End If
            Case Else
                If tas IsNot Nothing Then
                    ErrMsg("Bad Bracket Sequence in task """ & tas.Description & """")
                Else
                    ErrMsg("Bad Bracket Sequence")
                End If

        End Select

    End Function


    ' IgnoreReferences is for when we are evaluating whether the task is completable or not, but we don't have any refs yet
    Public Function PassRestrictions(ByVal arlRestrictions As RestrictionArrayList, Optional ByVal bIgnoreReferences As Boolean = False, Optional tas As clsTask = Nothing) As Boolean

        iRestNum = 0
        sRouteError = ""

        If arlRestrictions.Count = 0 Then
            Return True
        Else
            ' We have to check each combination of objects from our task
            ' e.g. "get %objects% from %object2%
            ' "get red ball and blue ball from box"
            ' get red ball from box
            ' get blue ball from box
            Return EvaluateRestrictionBlock(arlRestrictions, arlRestrictions.BracketSequence, bIgnoreReferences, tas)
        End If

    End Function


    Private Function InputMatchesObjects(ByVal task As clsTask, ByVal sInput As String, ByVal iNewRef As Integer, Optional ByVal bExcepts As Boolean = False, Optional ByVal bPlural As Boolean = False, Optional ByVal bSecondChance As Boolean = False) As Boolean

        Dim re As System.Text.RegularExpressions.Regex

        If Not bPlural Then
            re = New System.Text.RegularExpressions.Regex("^((?<all>all( .+)?)|(?<objects1>.+))( (except|but|apart from) (?<objects2>.+))?$", System.Text.RegularExpressions.RegexOptions.RightToLeft Or System.Text.RegularExpressions.RegexOptions.IgnoreCase)
            If re.IsMatch(sInput) Then
                For Each sGroupName As String In re.GetGroupNames
                    Select Case sGroupName

                        Case "all"
                            Dim sAll As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                            If sAll <> "" Then
                                If sAll = "all" Then
                                    ' User isn't refining 'all', so we need to populate list with all objects
                                    Dim iItemNum As Integer = 0
                                    For Each ob As clsObject In Adventure.htblObjects.SeenBy.Values
                                        task.NewReferences(iNewRef).Items.Add(New clsSingleItem)
                                        task.NewReferences(iNewRef).Items(iItemNum).MatchingPossibilities.Add(ob.Key)
                                        task.NewReferences(iNewRef).Items(iItemNum).bExplicitlyMentioned = False
                                        task.NewReferences(iNewRef).Items(iItemNum).sCommandReference = "all"
                                        iItemNum += 1
                                    Next
                                Else
                                    ' i.e. all balls
                                    ' object1 should be plural here, in which case we want to match any object with that as the plural, i.e. balls, cactii, sheep                          
                                    If Not InputMatchesObjects(task, sAll.Substring(4), iNewRef, , True) Then GoTo NextCheck ' Return False
                                End If
                            End If

                        Case "objects1"
                            Dim sObs As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                            If sObs <> "" AndAlso Not sObs.StartsWith("all ") Then
                                ' i.e. balls
                                ' object1 should be plural here, in which case we want to match any object with that as the plural, i.e. balls, cactii, sheep                          
                                If Not InputMatchesObjects(task, sObs, iNewRef, bExcepts, True) Then GoTo NextCheck
                            End If

                        Case "objects2"
                            Dim sExcepts As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                            If sExcepts <> "" Then
                                ' Need to go thru and remove any matching ref
                                InputMatchesObjects(task, sExcepts, iNewRef, True)
                            End If

                    End Select
                Next
                Return True
            End If

NextCheck:
            re = New System.Text.RegularExpressions.Regex("^(?<commaseparatedobjects>(.+), )*(?<object2>.+) and (?<object3>.+)$", System.Text.RegularExpressions.RegexOptions.IgnoreCase)
            If re.IsMatch(sInput) Then
                Dim m As System.Text.RegularExpressions.Match = re.Match(sInput)
                Dim iItemNum As Integer = 0
                If m.Groups("commaseparatedobjects").Length > 0 Then
                    For Each sObject As String In m.Groups("commaseparatedobjects").Value.TrimEnd(New Char() {","c, " "c}).Split(","c)
                        sObject = sObject.Trim
                        If Not InputMatchesObject(task, sObject, iNewRef, iItemNum, bExcepts) Then Return False
                        iItemNum += 1
                    Next
                End If
                If Not InputMatchesObject(task, m.Groups("object2").Value, iNewRef, iItemNum, bExcepts) Then Return False
                iItemNum += 1
                If Not InputMatchesObject(task, m.Groups("object3").Value, iNewRef, iItemNum, bExcepts) Then Return False
                Return True
            End If

        End If
        Return InputMatchesObject(task, sInput, iNewRef, 0, bExcepts, bPlural, bSecondChance)
    End Function


    Private Function InputMatchesObject(ByVal task As clsTask, ByVal sInput As String, ByVal iReferenceNum As Integer, ByVal iItemNum As Integer, Optional ByVal bExcepts As Boolean = False, Optional ByVal bPlural As Boolean = False, Optional ByVal bSecondChance As Boolean = False) As Boolean
        Dim bResult As Boolean = False
        Dim bAddedItem As Boolean = False

        If iItemNum = 0 AndAlso bPlural Then iItemNum = -1
        For Each ob As clsObject In Adventure.htblObjects.Values
            If System.Text.RegularExpressions.Regex.IsMatch(sInput, "^" & ob.sRegularExpressionString(, bPlural) & "$", System.Text.RegularExpressions.RegexOptions.IgnoreCase) Then
                bResult = True
                If Not bExcepts Then
                    If bPlural Then iItemNum += 1
                    If bPlural OrElse Not bAddedItem Then task.NewReferences(iReferenceNum).Items.Add(New clsSingleItem)
                    bAddedItem = True

                    task.NewReferences(iReferenceNum).Items(iItemNum).MatchingPossibilities.Add(ob.Key)
                    task.NewReferences(iReferenceNum).Items(iItemNum).bExplicitlyMentioned = True
                    task.NewReferences(iReferenceNum).Items(iItemNum).sCommandReference = sInput
                Else
                    For iItem As Integer = task.NewReferences(iReferenceNum).Items.Count - 1 To 0 Step -1
                        Dim itm As clsSingleItem = task.NewReferences(iReferenceNum).Items(iItem)
                        If itm.MatchingPossibilities.Contains(ob.Key) Then itm.MatchingPossibilities.Remove(ob.Key)
                        If itm.MatchingPossibilities.Count = 0 Then task.NewReferences(iReferenceNum).Items.RemoveAt(iItem)
                    Next
                End If
            End If
        Next

        If Not bResult AndAlso bSecondChance AndAlso task.HasObjectExistRestriction Then
            ' TODO - This needs to check that it is the correct 'must exist' check, rather than just any
            ' If our task has a check that objects should exist, return True as a match so the task can deal with that in it's restrictions
            'task.NewReferences(iReferenceNum).Items.Add(New clsSingleItem)
            'task.NewReferences(iReferenceNum).Items(0).MatchingPossibilities.Add("NonExistingObject")
            'task.NewReferences(iReferenceNum).Items(0).bExplicitlyMentioned = False
            'task.NewReferences(iReferenceNum).Items(0).sCommandReference = sInput
            Return True
        End If

        Return bResult

    End Function


    Private Function InputMatchesCharacters(ByVal task As clsTask, ByVal sInput As String, ByVal iNewRef As Integer, Optional ByVal bExcepts As Boolean = False, Optional ByVal bSecondChance As Boolean = False) As Boolean
        Dim re As System.Text.RegularExpressions.Regex

NextCheck:
        re = New System.Text.RegularExpressions.Regex("^(?<commaseparatedcharacters>(.+), )*(?<character2>.+) and (?<character3>.+)$", System.Text.RegularExpressions.RegexOptions.IgnoreCase)
        If re.IsMatch(sInput) Then
            Dim m As System.Text.RegularExpressions.Match = re.Match(sInput)
            Dim iItemNum As Integer = 0
            If m.Groups("commaseparatedcharacters").Length > 0 Then
                For Each sCharacter As String In m.Groups("commaseparatedcharacters").Value.TrimEnd(New Char() {","c, " "c}).Split(","c)
                    sCharacter = sCharacter.Trim
                    If Not InputMatchesCharacter(task, sCharacter, iNewRef, iItemNum, bExcepts) Then Return False
                    iItemNum += 1
                Next
            End If
            If Not InputMatchesCharacter(task, m.Groups("character2").Value, iNewRef, iItemNum, bExcepts) Then Return False
            iItemNum += 1
            If Not InputMatchesCharacter(task, m.Groups("character3").Value, iNewRef, iItemNum, bExcepts) Then Return False
            Return True
        End If

        Return InputMatchesCharacter(task, sInput, iNewRef, 0, bExcepts, bSecondChance)

    End Function


    Private Function InputMatchesLocation(ByVal task As clsTask, ByVal sInput As String, ByVal iReferenceNum As Integer, ByVal iItemNum As Integer, Optional ByVal bExcepts As Boolean = False, Optional ByVal bSecondChance As Boolean = False) As Boolean
        Dim bResult As Boolean = False
        Dim bAddedLoc As Boolean = False

        For Each l As clsLocation In Adventure.htblLocations.Values
            If System.Text.RegularExpressions.Regex.IsMatch(sInput, "^" & l.sRegularExpressionString() & "$", System.Text.RegularExpressions.RegexOptions.IgnoreCase) Then
                bResult = True
                If Not bExcepts Then
                    If Not bAddedLoc Then task.NewReferences(iReferenceNum).Items.Add(New clsSingleItem)
                    bAddedLoc = True

                    task.NewReferences(iReferenceNum).Items(iItemNum).MatchingPossibilities.Add(l.Key)
                    task.NewReferences(iReferenceNum).Items(iItemNum).bExplicitlyMentioned = True
                    task.NewReferences(iReferenceNum).Items(iItemNum).sCommandReference = sInput
                Else
                    For iItem As Integer = task.NewReferences(iReferenceNum).Items.Count - 1 To 0 Step -1
                        Dim itm As clsSingleItem = task.NewReferences(iReferenceNum).Items(iItem)
                        If itm.MatchingPossibilities.Contains(l.Key) Then itm.MatchingPossibilities.Remove(l.Key)
                        If itm.MatchingPossibilities.Count = 0 Then task.NewReferences(iReferenceNum).Items.RemoveAt(iItem)
                    Next
                End If
            End If
        Next

        If Not bResult AndAlso bSecondChance AndAlso task.HasLocationExistRestriction Then
            ' TODO - This needs to check that it is the correct 'must exist' check, rather than just any
            ' If our task has a check that objects should exist, return True as a match so the task can deal with that in it's restrictions
            'task.NewReferences(iReferenceNum).Items.Add(New clsSingleItem)
            'task.NewReferences(iReferenceNum).Items(0).MatchingPossibilities.Add("NonExistingObject")
            'task.NewReferences(iReferenceNum).Items(0).bExplicitlyMentioned = False
            'task.NewReferences(iReferenceNum).Items(0).sCommandReference = sInput
            Return True
        End If

        Return bResult
    End Function


    Private Function InputMatchesCharacter(ByVal task As clsTask, ByVal sInput As String, ByVal iReferenceNum As Integer, ByVal iItemNum As Integer, Optional ByVal bExcepts As Boolean = False, Optional ByVal bSecondChance As Boolean = False) As Boolean
        Dim bResult As Boolean = False
        Dim bAddedChar As Boolean = False

        For Each ch As clsCharacter In Adventure.htblCharacters.Values
            If System.Text.RegularExpressions.Regex.IsMatch(sInput, "^" & ch.sRegularExpressionString() & "$", System.Text.RegularExpressions.RegexOptions.IgnoreCase) Then
                bResult = True
                If Not bExcepts Then
                    If Not bAddedChar Then task.NewReferences(iReferenceNum).Items.Add(New clsSingleItem)
                    bAddedChar = True

                    task.NewReferences(iReferenceNum).Items(iItemNum).MatchingPossibilities.Add(ch.Key)
                    task.NewReferences(iReferenceNum).Items(iItemNum).bExplicitlyMentioned = True
                    task.NewReferences(iReferenceNum).Items(iItemNum).sCommandReference = sInput
                Else
                    For iItem As Integer = task.NewReferences(iReferenceNum).Items.Count - 1 To 0 Step -1
                        Dim itm As clsSingleItem = task.NewReferences(iReferenceNum).Items(iItem)
                        If itm.MatchingPossibilities.Contains(ch.Key) Then itm.MatchingPossibilities.Remove(ch.Key)
                        If itm.MatchingPossibilities.Count = 0 Then task.NewReferences(iReferenceNum).Items.RemoveAt(iItem)
                    Next
                End If
            End If
        Next

        If Not bResult AndAlso bSecondChance AndAlso task.HasCharacterExistRestriction Then
            ' TODO - This needs to check that it is the correct 'must exist' check, rather than just any
            ' If our task has a check that objects should exist, return True as a match so the task can deal with that in it's restrictions
            'task.NewReferences(iReferenceNum).Items.Add(New clsSingleItem)
            'task.NewReferences(iReferenceNum).Items(0).MatchingPossibilities.Add("NonExistingObject")
            'task.NewReferences(iReferenceNum).Items(0).bExplicitlyMentioned = False
            'task.NewReferences(iReferenceNum).Items(0).sCommandReference = sInput
            Return True
        End If

        Return bResult
    End Function


    ' If the command contains asterixes, strip them out, then add them in one by one until we (possibly) get a match
    Friend Function GetRegularExpression(ByVal sCommand As String, ByVal sInput As String, ByVal bRightToLeft As Boolean) As List(Of System.Text.RegularExpressions.Regex)
        Dim lRegExs As New List(Of System.Text.RegularExpressions.Regex)

        Try
            Dim options As System.Text.RegularExpressions.RegexOptions = System.Text.RegularExpressions.RegexOptions.IgnoreCase
            If bRightToLeft Then options = options Or System.Text.RegularExpressions.RegexOptions.RightToLeft

            If sCommand.Contains("*") Then
                sCommand = sCommand.Replace("**", "*")
                Dim iAsterixCount As Integer = 0
                Dim sTestCommand As String = sCommand

                While sTestCommand.Contains("*")
                    sTestCommand = Replace(sTestCommand, "*", "", , 1)
                    iAsterixCount += 1
                End While

                ' We should really try all combinations of wildcard removal.
                ' E.g. " * %object * " should give us " * %object% " and " %object% * "
                ' otherwise we may always end up matching the object name in *

                ' For * # * # * we want
                ' _ # _ # _
                ' * # _ # _
                ' _ # * # _
                ' _ # _ # *
                ' * # * # _
                ' * # _ # *
                ' _ # * # *
                ' * # * # *

                For iAsterix As Integer = iAsterixCount - 1 To -1 Step -1
                    sTestCommand = sCommand
                    For i As Integer = 0 To iAsterix
                        sTestCommand = Replace(sTestCommand, "*", "", , 1)
                    Next
                    Dim sPattern As String = ConvertToRE(sTestCommand)
                    Dim re As New System.Text.RegularExpressions.Regex(sPattern, options)
                    If sInput = "" OrElse re.IsMatch(sInput) Then lRegExs.Add(re) ' Return re
                Next

                Return lRegExs ' Nothing
            Else
                Dim sPattern As String = ConvertToRE(sCommand)
                lRegExs.Add(New System.Text.RegularExpressions.Regex(sPattern, options))
                Return lRegExs
            End If
        Catch ex As Exception
            ErrMsg("Error in command """ & sCommand & """", ex)
            Return lRegExs
        End Try
    End Function

    Private Function InputMatchesCommand(ByVal task As clsTask, ByVal sInput As String, ByVal sCommand As String, ByVal bSecondChance As Boolean, ByVal bRightToLeft As Boolean, Optional ByVal RegExs As List(Of System.Text.RegularExpressions.Regex) = Nothing) As Boolean

        If RegExs Is Nothing Then RegExs = GetRegularExpression(sCommand, sInput, bRightToLeft)

        For Each re As System.Text.RegularExpressions.Regex In RegExs
            Dim iNewReferences As Integer = 0

            ' Clear the references
            If task.NewReferences IsNot Nothing Then
                For i As Integer = 0 To task.NewReferences.Length - 1
                    task.NewReferences(i) = Nothing
                Next
            End If

            If Not re.IsMatch(sInput) Then GoTo DoesntMatch

            Dim sRefs As StringArrayList = task.References

            For Each sGroupName As String In re.GetGroupNames
                Select Case sGroupName
                    Case "objects"
                        iNewReferences += 1
                        task.NewReferences(iNewReferences - 1) = New clsNewReference(ReferencesType.Object)
                        task.NewReferences(iNewReferences - 1).ReferenceMatch = sGroupName

                        Dim sObjectsInput As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                        If Not InputMatchesObjects(task, sObjectsInput, iNewReferences - 1, , , bSecondChance) Then GoTo DoesntMatch ' Return False

                    Case "object1", "object2", "object3", "object4", "object5"
                        iNewReferences += 1
                        task.NewReferences(iNewReferences - 1) = New clsNewReference(ReferencesType.Object)
                        task.NewReferences(iNewReferences - 1).ReferenceMatch = sGroupName

                        Dim sObjectInput As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                        If Not InputMatchesObject(task, sObjectInput, iNewReferences - 1, 0, , , bSecondChance) Then GoTo DoesntMatch ' Return False

                    Case "characters"
                        iNewReferences += 1
                        task.NewReferences(iNewReferences - 1) = New clsNewReference(ReferencesType.Character)
                        task.NewReferences(iNewReferences - 1).ReferenceMatch = sGroupName

                        Dim sCharactersInput As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                        If Not InputMatchesCharacters(task, sCharactersInput, iNewReferences - 1, , bSecondChance) Then GoTo DoesntMatch

                    Case "character1", "character2", "character3", "character4", "character5"
                        iNewReferences += 1
                        task.NewReferences(iNewReferences - 1) = New clsNewReference(ReferencesType.Character)
                        task.NewReferences(iNewReferences - 1).ReferenceMatch = sGroupName

                        Dim sCharacterInput As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                        If Not InputMatchesCharacter(task, sCharacterInput, iNewReferences - 1, 0, , bSecondChance) Then GoTo DoesntMatch

                    Case "location1", "location2", "location3", "location4", "location5"
                        iNewReferences += 1
                        task.NewReferences(iNewReferences - 1) = New clsNewReference(ReferencesType.Location)
                        task.NewReferences(iNewReferences - 1).ReferenceMatch = sGroupName

                        Dim sLocationInput As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                        If Not InputMatchesLocation(task, sLocationInput, iNewReferences - 1, 0, , bSecondChance) Then GoTo DoesntMatch

                    Case "item1", "item2", "item3", "item4", "item5"
                        iNewReferences += 1
                        task.NewReferences(iNewReferences - 1) = New clsNewReference(ReferencesType.Item)
                        task.NewReferences(iNewReferences - 1).ReferenceMatch = sGroupName

                        Dim sItemInput As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                        If Not (InputMatchesObjects(task, sItemInput, iNewReferences - 1, , , bSecondChance) OrElse InputMatchesCharacter(task, sItemInput, iNewReferences - 1, 0, , bSecondChance) OrElse InputMatchesLocation(task, sItemInput, iNewReferences - 1, 0, , bSecondChance)) Then GoTo DoesntMatch

                    Case "direction1", "direction2", "direction3", "direction4", "direction5"
                        iNewReferences += 1
                        task.NewReferences(iNewReferences - 1) = New clsNewReference(ReferencesType.Direction)
                        task.NewReferences(iNewReferences - 1).ReferenceMatch = sGroupName

                        Dim sDirInput As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                        With task.NewReferences(iNewReferences - 1)
                            Dim sDirTest As String = ""
                            For dr As DirectionsEnum = DirectionsEnum.North To DirectionsEnum.NorthWest
                                sDirTest = DirectionRE(dr, True, True)
                                If System.Text.RegularExpressions.Regex.IsMatch(sDirInput, "^" & sDirTest & "$") Then
                                    .Items.Add(New clsSingleItem)
                                    .Items(0).MatchingPossibilities.Add(dr.ToString)
                                    .Items(0).sCommandReference = sDirInput
                                    Exit For
                                End If
                            Next
                        End With

                    Case "number1", "number2", "number3", "number4", "number5"
                        iNewReferences += 1
                        task.NewReferences(iNewReferences - 1) = New clsNewReference(ReferencesType.Number)
                        task.NewReferences(iNewReferences - 1).ReferenceMatch = sGroupName

                        Dim sNumber As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                        With task.NewReferences(iNewReferences - 1)
                            If System.Text.RegularExpressions.Regex.IsMatch(sNumber, "^-?[0-9]+$") Then
                                Dim iRef As Integer = Math.Max(SafeInt(sGroupName.Replace("number", "")) - 1, 0)
                                .Items.Add(New clsSingleItem)
                                .Items(0).MatchingPossibilities.Add(sNumber)
                                .Items(0).sCommandReference = sNumber
                            End If
                        End With

                    Case "text1", "text2", "text3", "text4", "text5"
                        iNewReferences += 1
                        task.NewReferences(iNewReferences - 1) = New clsNewReference(ReferencesType.Text)
                        task.NewReferences(iNewReferences - 1).ReferenceMatch = sGroupName

                        Dim sText As String = re.Match(sInput).Groups(sGroupName).Value.Trim
                        With task.NewReferences(iNewReferences - 1)
                            If System.Text.RegularExpressions.Regex.IsMatch(sText, "^.*$") Then
                                Dim iRef As Integer = Math.Max(SafeInt(sGroupName.Replace("text", "")) - 1, 0)
                                .Items.Add(New clsSingleItem)
                                .Items(0).MatchingPossibilities.Add(sText)
                                .Items(0).sCommandReference = sText
                                Adventure.sReferencedText(iRef) = sText
                            End If
                        End With

                End Select

            Next

            Return True
DoesntMatch:
            If sCommand.Contains("% %") AndAlso Not re.RightToLeft Then
                ' Ok, we have 2 references back to back.  Try a match from right to left
                Return InputMatchesCommand(task, sInput, sCommand, bSecondChance, True)
            End If
            If Not bSecondChance AndAlso Not htblSecondChanceTasks.ContainsKey(task.Key) Then
                If iNewReferences > 0 AndAlso iNewReferences <= task.NewReferences.Length AndAlso (
                    (task.NewReferences(iNewReferences - 1).ReferenceType = ReferencesType.Object AndAlso task.HasObjectExistRestriction) _
                    OrElse (task.NewReferences(iNewReferences - 1).ReferenceType = ReferencesType.Character AndAlso task.HasCharacterExistRestriction) _
                    OrElse (task.NewReferences(iNewReferences - 1).ReferenceType = ReferencesType.Location AndAlso task.HasLocationExistRestriction)) Then
                    htblSecondChanceTasks.Add(task, task.Key)
                End If
            End If
        Next
        Return False
    End Function

    Private Function CopyRef(ByVal ref As clsNewReference) As clsNewReference

        Dim nr As clsNewReference = Nothing

        If ref IsNot Nothing Then
            nr = New clsNewReference(ref.ReferenceType)
            For iItem As Integer = 0 To ref.Items.Count - 1
                Dim itm As New clsSingleItem
                itm.MatchingPossibilities = ref.Items(iItem).MatchingPossibilities.Clone
                itm.bExplicitlyMentioned = ref.Items(iItem).bExplicitlyMentioned
                itm.sCommandReference = ref.Items(iItem).sCommandReference
                nr.Items.Add(itm)
            Next
            nr.Index = ref.Index
            nr.ReferenceMatch = ref.ReferenceMatch
        End If
        Return nr
    End Function

    Private Sub RefineMatchingPossibilitesUsingRestrictions(ByVal task As clsTask)
        DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Checking scope: Applicable")

        Dim NewRefs(task.NewReferencesWorking.Length - 1) As clsNewReference

        Dim lAdded(task.NewReferencesWorking.Length - 1) As Generic.List(Of String)

        For i As Integer = 0 To task.NewReferencesWorking.Length - 1
            lAdded(i) = New Generic.List(Of String)
            If task.NewReferencesWorking(i) IsNot Nothing Then
                NewRefs(i) = New clsNewReference(task.NewReferencesWorking(i).ReferenceType)
                NewRefs(i).Index = task.NewReferencesWorking(i).Index
                NewRefs(i).ReferenceMatch = task.NewReferencesWorking(i).ReferenceMatch
            End If
        Next

        ' We have to try every combination of references against all the others to see if it is a successful combination      
        If task.NewReferencesWorking IsNot Nothing AndAlso task.NewReferencesWorking.Length > 0 AndAlso task.NewReferencesWorking(0) IsNot Nothing Then

            For Each itm0 As clsSingleItem In task.NewReferencesWorking(0).Items

                Dim bAddedItem0 As Boolean = False
                ReDim NewReferences(0)
                NewReferences(0) = New clsNewReference(task.NewReferencesWorking(0).ReferenceType)
                NewReferences(0).Index = task.NewReferencesWorking(0).Index
                NewReferences(0).ReferenceMatch = task.NewReferencesWorking(0).ReferenceMatch
                Dim itmOut0 As New clsSingleItem
                itmOut0.sCommandReference = itm0.sCommandReference

                For Each sKey0 As String In itm0.MatchingPossibilities

                    NewReferences(0).Items.Clear()
                    Dim itmSingle0 As New clsSingleItem
                    itmSingle0.MatchingPossibilities.Add(sKey0)
                    NewReferences(0).Items.Add(itmSingle0)

                    If task.NewReferencesWorking.Length > 1 Then

                        For Each itm1 As clsSingleItem In task.NewReferencesWorking(1).Items

                            Dim bAddedItem1 As Boolean = False
                            ReDim Preserve NewReferences(1)
                            NewReferences(1) = New clsNewReference(task.NewReferencesWorking(1).ReferenceType)
                            NewReferences(1).Index = task.NewReferencesWorking(1).Index
                            NewReferences(1).ReferenceMatch = task.NewReferencesWorking(1).ReferenceMatch
                            Dim itmOut1 As New clsSingleItem
                            itmOut1.sCommandReference = itm1.sCommandReference

                            For Each sKey1 As String In itm1.MatchingPossibilities

                                NewReferences(1).Items.Clear()
                                Dim itmSingle1 As New clsSingleItem
                                itmSingle1.MatchingPossibilities.Add(sKey1)
                                NewReferences(1).Items.Add(itmSingle1)

                                If task.NewReferencesWorking.Length > 2 Then
                                    ' TODO, when more than 2 refs...
                                Else
                                    If PassRestrictions(task.arlRestrictions) Then
                                        If Not bAddedItem0 Then NewRefs(0).Items.Add(itmOut0)
                                        If Not bAddedItem1 Then NewRefs(1).Items.Add(itmOut1)
                                        If Not lAdded(0).Contains(sKey0) Then itmOut0.MatchingPossibilities.Add(sKey0)
                                        If Not lAdded(1).Contains(sKey1) Then itmOut1.MatchingPossibilities.Add(sKey1)
                                        bAddedItem0 = True
                                        bAddedItem1 = True
                                        If Not lAdded(0).Contains(sKey0) Then lAdded(0).Add(sKey0)
                                        If Not lAdded(1).Contains(sKey1) Then lAdded(1).Add(sKey1)
                                    End If
                                End If
                            Next sKey1

                        Next itm1
                    Else
                        If PassRestrictions(task.arlRestrictions) Then
                            If Not bAddedItem0 AndAlso Not lAdded(0).Contains(sKey0) Then NewRefs(0).Items.Add(itmOut0)
                            itmOut0.MatchingPossibilities.Add(sKey0)
                            bAddedItem0 = True
                            If Not lAdded(0).Contains(sKey0) Then lAdded(0).Add(sKey0)
                        End If
                    End If

                Next sKey0
            Next itm0

        End If

        ' Check to see if this refined our possibilities to unique items
        Dim bCheckNextScope As Boolean = False

        For iNR As Integer = 0 To NewRefs.Length - 1 ' Each nr As clsNewReference In NewRefs
            Dim nr As clsNewReference = NewRefs(iNR)
            Dim bResetRef As Boolean = False
            If nr IsNot Nothing AndAlso nr.Items IsNot Nothing Then
                If nr.Items.Count = 0 Then
                    bCheckNextScope = True
                    bResetRef = True
                End If
                For iI As Integer = 0 To nr.Items.Count - 1 ' Each itm As clsSingleItem In nr.Items
                    Dim itm As clsSingleItem = nr.Items(iI)
                    If itm.MatchingPossibilities.Count = 0 Then
                        bCheckNextScope = True
                        ' Reset refs to original
                        If task.NewReferencesWorking(iNR).Items.Count > iI Then
                            itm.MatchingPossibilities = task.NewReferencesWorking(iNR).Items(iI).MatchingPossibilities.Clone
                            bResetRef = True
                        End If
                    ElseIf itm.MatchingPossibilities.Count > 1 Then
                        bCheckNextScope = True
                        ' Leave existing refs to further refine by visibility
                    End If
                Next
                If bResetRef Then NewRefs(iNR) = CopyRef(task.NewReferences(iNR))
            End If
        Next
        If bCheckNextScope Then
            DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Checking scope: Visible")

            For Each nr As clsNewReference In NewRefs
                If nr IsNot Nothing Then
                    For Each itm As clsSingleItem In nr.Items
                        For i As Integer = itm.MatchingPossibilities.Count - 1 To 0 Step -1
                            Select Case nr.ReferenceType
                                Case ReferencesType.Object
                                    If Not Adventure.htblObjects(itm.MatchingPossibilities(i)).IsVisibleTo(Adventure.Player.Key) Then itm.MatchingPossibilities.RemoveAt(i)
                                Case ReferencesType.Character
                                    If Not Adventure.htblCharacters(itm.MatchingPossibilities(i)).CanSeeCharacter(Adventure.Player.Key) Then itm.MatchingPossibilities.RemoveAt(i)
                                Case ReferencesType.Direction, ReferencesType.Number, ReferencesType.Text
                                    ' Don't think these are applicable       
                                Case ReferencesType.Location
                                    ' Doesn't make sense to restrict by visible for Location
                                Case ReferencesType.Item
                                    Dim sItemKey As String = itm.MatchingPossibilities(i)
                                    Dim ob As clsObject = Nothing
                                    Dim ch As clsCharacter = Nothing
                                    If Adventure.htblObjects.TryGetValue(sItemKey, ob) Then
                                        If Not ob.IsVisibleTo(Adventure.Player.Key) Then itm.MatchingPossibilities.RemoveAt(i)
                                    ElseIf Adventure.htblCharacters.TryGetValue(sItemKey, ch) Then
                                        If Not ch.CanSeeCharacter(Adventure.Player.Key) Then itm.MatchingPossibilities.RemoveAt(i)
                                    End If
                                Case Else
                                    TODO("Refine visible possibilites for " & nr.ReferenceType.ToString & " references")
                            End Select
                        Next i
                    Next itm
                End If
            Next nr
        End If

        ' Check to see if this refined our possibilities to unique items
        bCheckNextScope = False

        For iNR As Integer = 0 To NewRefs.Length - 1
            Dim nr As clsNewReference = NewRefs(iNR)
            Dim bResetRef As Boolean = False
            If nr IsNot Nothing AndAlso nr.Items IsNot Nothing Then
                If nr.Items.Count = 0 Then
                    bCheckNextScope = True
                    bResetRef = True
                End If
                For iI As Integer = nr.Items.Count - 1 To 0 Step -1
                    Dim itm As clsSingleItem = nr.Items(iI)
                    If itm.MatchingPossibilities.Count = 0 Then
                        nr.Items.RemoveAt(iI)
                        If nr.Items.Count = 0 Then
                            bCheckNextScope = True
                            ' Reset refs to original
                            itm.MatchingPossibilities = task.NewReferencesWorking(iNR).Items(iI).MatchingPossibilities.Clone
                            bResetRef = True
                        Else
                            ' We still have some items, so at least our Visible scope reduced from previous
                        End If
                    ElseIf itm.MatchingPossibilities.Count > 1 Then
                        ' Leave existing refs to further refine by visibility
                        bCheckNextScope = True ' JCW - Added 13/08/12 - not sure how we would be leaving to further refine otherwise...
                    End If
                Next
                If bResetRef Then NewRefs(iNR) = CopyRef(task.NewReferences(iNR))
            End If
        Next

        If bCheckNextScope Then
            DebugPrint(ItemEnum.Task, task.Key, DebugDetailLevelEnum.High, "Checking scope: Seen")

            ' Remove any unseen references from our set
            For Each nr As clsNewReference In NewRefs
                If nr IsNot Nothing Then
                    For Each itm As clsSingleItem In nr.Items
                        For i As Integer = itm.MatchingPossibilities.Count - 1 To 0 Step -1
                            Select Case nr.ReferenceType
                                Case ReferencesType.Object
                                    If Not Adventure.htblObjects(itm.MatchingPossibilities(i)).SeenBy(Adventure.Player.Key) Then itm.MatchingPossibilities.RemoveAt(i)
                                Case ReferencesType.Character
                                    If Not Adventure.htblCharacters(itm.MatchingPossibilities(i)).SeenBy(Adventure.Player.Key) Then itm.MatchingPossibilities.RemoveAt(i)
                                Case ReferencesType.Location
                                    If Not Adventure.htblLocations(itm.MatchingPossibilities(i)).SeenBy(Adventure.Player.Key) Then itm.MatchingPossibilities.RemoveAt(i)
                            End Select
                        Next i
                    Next itm
                End If
            Next nr
        End If

        bCheckNextScope = False

        ' Check to see if this refined our possibilities to unique items
        For iNR As Integer = 0 To NewRefs.Length - 1
            Dim nr As clsNewReference = NewRefs(iNR)
            Dim bResetRef As Boolean = False
            If nr IsNot Nothing AndAlso nr.Items IsNot Nothing Then
                If nr.Items.Count = 0 Then
                    bCheckNextScope = True
                    bResetRef = True
                End If
                For iI As Integer = 0 To nr.Items.Count - 1
                    Dim itm As clsSingleItem = nr.Items(iI)
                    If itm.MatchingPossibilities.Count = 0 Then
                        bCheckNextScope = True
                        bResetRef = True
                    ElseIf itm.MatchingPossibilities.Count > 1 Then
                        bCheckNextScope = True
                        ' Leave existing refs to further refine by visibility
                    End If
                Next
                If bResetRef Then NewRefs(iNR) = CopyRef(task.NewReferences(iNR))
            End If
        Next

        task.NewReferencesWorking = NewRefs
    End Sub


    Private Function GetGeneralTask(ByVal sInput As String, ByVal iMinimumPriority As Integer, ByVal bSecondChance As Boolean) As String
        Dim iPriorityFail As Integer = Integer.MaxValue ' Lowest priority task that matches input and has output
        Dim sNoRefTask As String = "" ' We may match on a task, but find no references.  If so, at least return this rather than no task at all

        GetGeneralTask = Nothing
        If Not bSecondChance Then htblSecondChanceTasks.Clear()
        If iMinimumPriority > 0 AndAlso iMinimumPriority < iPriorityFail Then iPriorityFail = iMinimumPriority ' Because if we're continuing on from an earlier execution, we shouldn't run lower priority tasks

        Try
            sAmbTask = Nothing
            Dim htblTasks As TaskHashTable = CType(IIf(bSecondChance, htblSecondChanceTasks, htblCompleteableGeneralTasks), TaskHashTable)

            For Each tk As TaskKey In listTaskKeys
                If htblTasks.ContainsKey(tk.sTaskKey) Then
                    Dim tas As clsTask = htblTasks(tk.sTaskKey)

                    If tas.Priority >= iMinimumPriority AndAlso Not (tas.LowPriority AndAlso tas.Priority > iPriorityFail) Then

                        ' Don't run Library tasks if this is a continuation task (v4)
                        If iMinimumPriority > 0 AndAlso tas.IsLibrary AndAlso Adventure.dVersion < 5 Then
                            GoTo FoundTask
                        End If

                        For iCom As Integer = 0 To tas.arlCommands.Count - 1
                            Dim sCom As String = tas.arlCommands(iCom)

                            If InputMatchesCommand(tas, sInput, sCom, bSecondChance, False, tas.RegExs(iCom)) Then

                                DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.Medium, "Task '" & tas.Description & "' matches input.")
                                iMatchedTaskCommand = iCom

                                ' Now see if user typed unique references (or none at all) which allow us to run the task                                

                                Dim bOkToRun As Boolean = True

                                tas.NewReferencesWorking = tas.CopyNewRefs(tas.NewReferences)
                                ' If any refs are vague, refine using restrictions
                                Dim bMatchesPreRefine As Boolean = True
                                For Each nr As clsNewReference In tas.NewReferences
                                    If nr IsNot Nothing Then
                                        For Each itm As clsSingleItem In nr.Items
                                            If itm.MatchingPossibilities.Count <> 1 Then
                                                bMatchesPreRefine = False
                                                Exit For
                                            End If
                                        Next itm
                                    End If
                                Next nr

                                ' Remove any references that don't pass the restrictions from our set
                                RefineMatchingPossibilitesUsingRestrictions(tas)

                                ' If we still have at least one matching possibility in each reference item, then run this task, else we don't pass
                                For Each nr As clsNewReference In tas.NewReferencesWorking
                                    If nr IsNot Nothing Then
                                        For Each itm As clsSingleItem In nr.Items
                                            If itm.MatchingPossibilities.Count = 0 Then
                                                ' Can we fall back to previous refs?  We need to fire this task rather than fall thru to not understood...
                                                If bMatchesPreRefine Then
                                                    tas.NewReferencesWorking = tas.CopyNewRefs(tas.NewReferences)
                                                Else
                                                    DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.High, "No matches found.")
                                                    ' Should we perhaps fall back anyway and prompt for disambiguity - unless a lower priority one matches.  Hmm.... may need
                                                    ' to run the whole thing again with a different parameter.
                                                    If sNoRefTask = "" Then sNoRefTask = tas.Key
                                                    bOkToRun = False
                                                End If
                                                Exit For
                                            End If
                                            If itm.MatchingPossibilities.Count > 1 Then
                                                If sAmbTask Is Nothing Then
                                                    ' We have an ambiguity that needs to be resolved
                                                    DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.High, "Multiple matches.  Prompt for ambiguity.")
                                                    sAmbTask = tas.Key
                                                Else
                                                    DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.High, "Multiple matches, but we already have an ambiguity.")
                                                End If
                                                bOkToRun = False
                                            End If
                                        Next itm
                                    End If
                                Next nr

                                If bOkToRun Then
                                    ' Woo-hoo, we have a unique match!
                                    If tas.References.Count > 0 Then
                                        DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.High, "Command matches without ambiguity.")
                                    Else
                                        DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.High, "Command matches.")
                                    End If

                                    NewReferences = tas.NewReferencesWorking
                                    Dim bDoesThisPass As Boolean = PassRestrictions(tas.arlRestrictions, , tas)

                                    If Not bDoesThisPass AndAlso tas.FailOverride.ToString <> "" AndAlso sRestrictionText = "" AndAlso ContainsWord(sInput, "all") Then
                                        sRestrictionText = tas.FailOverride.ToString
                                    End If

                                    ' If we pass restrictions, or if we haven't yet passed but we have output

                                    If bDoesThisPass OrElse sRestrictionText <> "" Then
                                        If bDoesThisPass Then
                                            If tas.Priority > iPriorityFail Then
                                                DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.Medium, "Task passes restrictions and overrides previous failing task output")
                                            Else
                                                DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.Medium, "Task passes restrictions.")
                                            End If
                                        ElseIf sRestrictionText <> "" Then
                                            DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.Medium, "Task doesn't pass restrictions, but is current highest priority failing task with restriction output.")
                                        End If

                                        GetGeneralTask = tas.Key
                                        If sRestrictionText <> "" Then iPriorityFail = tas.Priority

                                        DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.High, "Task priority: " & tas.Priority)

                                        If bDoesThisPass OrElse Adventure.TaskExecution = clsAdventure.TaskExecutionEnum.HighestPriorityTask Then GoTo FoundTask

                                    Else
                                        DebugPrint(ItemEnum.Task, tas.Key, DebugDetailLevelEnum.Medium, "Task does not pass restrictions.")
                                    End If

                                    Exit For ' task
                                End If

                            End If
                        Next
                    End If
                End If
            Next
FoundTask:

            If GetGeneralTask Is Nothing AndAlso sNoRefTask <> "" Then GetGeneralTask = sNoRefTask

            If GetGeneralTask Is Nothing AndAlso sAmbTask Is Nothing AndAlso Not bSecondChance Then
                ' Ok, no luck.  Let's go back and see if one of our 'exist' tasks worked
                DebugPrint(ItemEnum.Task, "", DebugDetailLevelEnum.Medium, "No matches found.  Checking again using existance.")
                Return GetGeneralTask(sInput, iMinimumPriority, True)
            End If
        Catch ex As Exception
            ErrMsg("GetGeneralTask Error", ex)
        End Try
    End Function


    Private Function IsNoun(ByVal sName As String) As Boolean
        Dim re As New System.Text.RegularExpressions.Regex("")
        For Each ob As clsObject In Adventure.htblObjects.Values
            Dim sOb As String = ob.sRegularExpressionString
            If System.Text.RegularExpressions.Regex.IsMatch(sName, "^" & sOb & "s?$") Then Return True
        Next ob
        Return False
    End Function


    ''' <summary>
    ''' Converts a lazy Advanced Command to strict format, removing possibility for double spaces
    ''' </summary>
    ''' <param name="sCommand"></param>
    ''' <returns></returns>
    ''' <remarks></remarks>
    Public Function CorrectCommand(ByVal sCommand As String) As String
        Dim sNewCommand As String = ProcessBlock(sCommand)
        If sNewCommand <> sCommand Then
            Debug.WriteLine(String.Format("Converted ""{0}"" to ""{1}""", sCommand, sNewCommand))
            sCommand = sCommand
        End If
        Return sNewCommand
    End Function

    ' E.g. from "get {the} ball" > "get ", from "{the} ball" > "the", " ball" > "" 
    Private Function GetSubBlock(ByRef sBlock As String) As String
        Dim iDepth As Integer = 0
        Dim sNewBlock As String = ""

        For i As Integer = 0 To sBlock.Length - 1
            sNewBlock &= sBlock(i)
            Select Case sBlock(i)
                Case "{"c, "["c
                    If iDepth = 0 AndAlso sNewBlock <> sBlock(i) Then
                        sBlock = sRight(sBlock, sBlock.Length - sNewBlock.Length + 1)
                        Return sLeft(sNewBlock, i)
                    End If
                    iDepth += 1
                Case "]"c, "}"c
                    iDepth -= 1
                    If iDepth = 0 Then
                        sBlock = sRight(sBlock, sBlock.Length - sNewBlock.Length)
                        Return sNewBlock
                    End If
                Case "/"c
                    If iDepth = 0 Then
                        sBlock = sRight(sBlock, sBlock.Length - sNewBlock.Length)
                        Return sNewBlock
                    End If
            End Select
        Next
        sBlock = ""
        Return sNewBlock
    End Function

    ' Returns True if any part of the block is mandatory (either inside [] or not in brackets at all)
    Private Function ContainsMandatoryText(ByVal sBlock As String) As Boolean
        If sBlock = "" Then Return False

        Dim iLevel As Integer = 0
        For i As Integer = 0 To sBlock.Length - 1
            Select Case sBlock(i)
                Case " "c
                    ' Ignore
                Case "{"c
                    iLevel += 1
                Case "}"c
                    iLevel -= 1
                Case Else
                    If iLevel = 0 Then Return True
            End Select
        Next
        Return False
    End Function

    ' A block should be the complete entry between two brackets, or between a bracket and a slash
    Private Function ProcessBlockOld(ByVal sBlock As String) As String
        Dim sAfter As String = sBlock
        Dim sNextBlock As String = ""
        Dim sBefore As String = ""

        Do
            sNextBlock = GetSubBlock(sAfter)
            If sNextBlock <> "" Then
                If sNextBlock.StartsWith("{") Then
                    ' */ "] {#} {" => "]{ #}{"
                    ' "] {#} [" => ?
                    ' "} {#} [" => ?
                    ' "} {#} {" => ? /*
                    ' "{#} " => "{# }" if block starts with open bracket
                    ' " {#} " => " {# }"
                    ' "}{#} " => "}{# }"        -- should this be " }{#} " => " }{# }" ?
                    If (sBefore = "" OrElse sRight(sBefore, 1) = " " OrElse sRight(sBefore, 1) = "}") AndAlso sAfter.StartsWith(" ") Then
                        If sNextBlock.Contains("/") Then
                            sNextBlock = "{[" & sLeft(sNextBlock.Substring(1), sNextBlock.Length - 2) & "] }"
                        Else
                            sNextBlock = sLeft(sNextBlock, sNextBlock.Length - 1) & " }"
                        End If
                        sAfter = sRight(sAfter, sAfter.Length - 1)
                    End If

                    ' End block
                    ' " {#}" => "{ #}" or "{ [#/#]}
                    If sBefore.EndsWith(" ") AndAlso sAfter = "" Then
                        If sNextBlock.Contains("/") Then
                            sNextBlock = "{ [" & sLeft(sNextBlock.Substring(1), sNextBlock.Length - 2) & "]}"
                        Else
                            sNextBlock = "{ " & sLeft(sNextBlock.Substring(1), sNextBlock.Length - 1)
                        End If
                        sBefore = sLeft(sBefore, sBefore.Length - 1)
                    End If
                    sBefore &= "{" & ProcessBlock(sMid(sNextBlock, 2, sNextBlock.Length - 2)) & "}"
                ElseIf sNextBlock.StartsWith("[") Then
                    sBefore &= "[" & ProcessBlock(sMid(sNextBlock, 2, sNextBlock.Length - 2)) & "]"
                ElseIf sNextBlock.EndsWith("/") Then
                    sBefore &= ProcessBlock(sLeft(sNextBlock, sNextBlock.Length - 1)) & "/"
                Else
                    sBefore &= sNextBlock
                End If
            End If
        Loop While sAfter <> ""
        Return sBefore
    End Function

    ' A block should be the complete entry between two brackets, or between a bracket and a slash
    Private Function ProcessBlock(ByVal sBlock As String) As String
        Dim sAfter As String = sBlock
        Dim sNextBlock As String = ""
        Dim sBefore As String = ""

        Do
            sNextBlock = GetSubBlock(sAfter)
            If sNextBlock <> "" Then
                If sNextBlock.StartsWith("{") Then
                    ' */ "] {#} {" => "]{ #}{"
                    ' "] {#} [" => ?
                    ' "} {#} [" => ?
                    ' "} {#} {" => ? /*
                    ' "{#} " => "{# }" if block starts with open bracket
                    ' " {#} " => " {# }"
                    ' "}{#} " => "}{# }"        -- should this be " }{#} " => " }{# }" ?

                    Dim bContainsMandatory As Boolean = ContainsMandatoryText(sAfter)
                    If bContainsMandatory AndAlso sAfter.StartsWith(" ") Then
                        ' If before final [] block then _{x}_ => _{x_}
                        If sBefore = "" OrElse sBefore.EndsWith(" ") OrElse sBefore.EndsWith("}") Then
                            If sNextBlock.Contains("/") Then
                                sNextBlock = "{[" & sLeft(sNextBlock.Substring(1), sNextBlock.Length - 2) & "] }"
                            Else
                                sNextBlock = sLeft(sNextBlock, sNextBlock.Length - 1) & " }"
                            End If
                            sAfter = sRight(sAfter, sAfter.Length - 1)
                        End If
                    ElseIf Not bContainsMandatory AndAlso sBefore.EndsWith(" ") Then
                        ' If after final [] block then _{x}_ => {_x}_
                        If sNextBlock.Contains("/") Then
                            sNextBlock = "{ [" & sLeft(sNextBlock.Substring(1), sNextBlock.Length - 2) & "]}"
                        Else
                            sNextBlock = "{ " & sLeft(sNextBlock.Substring(1), sNextBlock.Length - 1)
                        End If
                        sBefore = sLeft(sBefore, sBefore.Length - 1)
                    End If

                    ' End block
                    ' " {#}" => "{ #}" or "{ [#/#]}
                    If sBefore.EndsWith(" ") AndAlso sAfter = "" Then
                        If sNextBlock.Contains("/") Then
                            sNextBlock = "{ [" & sLeft(sNextBlock.Substring(1), sNextBlock.Length - 2) & "]}"
                        Else
                            sNextBlock = "{ " & sLeft(sNextBlock.Substring(1), sNextBlock.Length - 1)
                        End If
                        sBefore = sLeft(sBefore, sBefore.Length - 1)
                    End If
                    sBefore &= "{" & ProcessBlock(sMid(sNextBlock, 2, sNextBlock.Length - 2)) & "}"
                ElseIf sNextBlock.StartsWith("[") Then
                    sBefore &= "[" & ProcessBlock(sMid(sNextBlock, 2, sNextBlock.Length - 2)) & "]"
                ElseIf sNextBlock.EndsWith("/") Then
                    sBefore &= ProcessBlock(sLeft(sNextBlock, sNextBlock.Length - 1)) & "/"
                Else
                    sBefore &= sNextBlock
                End If
            End If
        Loop While sAfter <> ""
        Return sBefore
    End Function

    Private sDirectionsRE As String
    Public Function ConvertToRE(ByVal sCommand As String, Optional ByVal bReplaceRefs As Boolean = True, Optional ByVal bTerminate As Boolean = True) As String
        Dim sC As String = sCommand

        ' Convert any special RE characters
        sC = sC.Replace("+", "\+").Replace("(", "\(").Replace(")", "\)")

        ' Convert wildcards
        sC = sC.Replace(" * ", " ([[#]] )?")
        sC = sC.Replace("* ", "([[#]] )?")
        sC = sC.Replace(" *", "( [[#]])?")
        sC = sC.Replace("*", "[[#]]")
        sC = sC.Replace("[[#]]", ".*?")

        sC = sC.Replace("_", " ")

        ' Convert optional text and mandatory text
        sC = sC.Replace("{", "(").Replace("}", ")?").Replace("[", "(").Replace("]", ")").Replace("/", "|")

        If bReplaceRefs Then
            ' Replace references
            If sDirectionsRE Is Nothing Then
                For eDirection As DirectionsEnum = DirectionsEnum.North To DirectionsEnum.NorthWest
                    sDirectionsRE &= Adventure.sDirectionsRE(eDirection).ToLower.Replace("/", "|")
                    If eDirection <> DirectionsEnum.NorthWest Then sDirectionsRE &= "|"
                Next
            End If
            sC = sC.Replace("%direction%", "(?<direction>" & sDirectionsRE & ")")
            ' TODO - replace above with custom names if changed

            sC = sC.Replace("%objects%", "(?<objects>.+?)")
            sC = sC.Replace("%characters%", "(?<characters>.+?)")

            For iref As Integer = 1 To 5
                sC = sC.Replace("%object" & iref & "%", "(?<object" & iref & ">.+?)")
                sC = sC.Replace("%character" & iref & "%", "(?<character" & iref & ">.+?)")
                sC = sC.Replace("%number" & iref & "%", "(?<number" & iref & ">-?[0-9]+)")
                sC = sC.Replace("%text" & iref & "%", "(?<text" & iref & ">.+?)")
                sC = sC.Replace("%location" & iref & "%", "(?<location" & iref & ">.+?)")
                sC = sC.Replace("%item" & iref & "%", "(?<item" & iref & ">.+?)")
                sC = sC.Replace("%direction" & iref & "%", "(?<direction" & iref & ">" & sDirectionsRE & ")")
            Next
        End If
        sC = sC.Trim

        If bTerminate Then
            Return "^" & sC & "$"
        Else
            Return sC
        End If
    End Function

    Private Function GetHtblKeys(ByVal sRefType As String, ByVal iRef As Integer) As System.Collections.ICollection
        Select Case sRefType
            Case "ReferencedObject1", "ReferencedObject2", "ReferencedObject3", "ReferencedObject4", "ReferencedObject5", "ReferencedObjects"
                Return Adventure.htblObjects.Keys
            Case "ReferencedDirection", "number", "character", "text"
                Return New Collection
            Case Else
                iRef = iRef
        End Select

        Return Nothing
    End Function

    Private Function GetHtblAtPos(ByVal htbl As Hashtable, ByVal iPos As Integer) As Object
        Dim iCounter As Integer = 0
        For Each item As Object In htbl.Values
            iCounter += 1
            If iCounter = iPos Then Return item
        Next

        Return Nothing
    End Function


    ' Returns the number of references in a task
    Private Function NumberOfRefs(ByVal cTask As clsTask) As Integer
        Dim sTokens() As String = cTask.arlCommands(0).Split(" "c)
        Dim iNoRefs As Integer = 0

        For Each sToken As String In sTokens
            Select Case sToken
                Case "%character%", "%characters", "%direction%", "%number%", "%numbers%", "%object1%", "%object2%", "%object3%", "%object4%", "%object5%", "%objects%", "%text%", "%location%"
                    iNoRefs += 1
            End Select
        Next

        Return iNoRefs
    End Function

#If Not Adravalon Then
    Private Sub tmrEvents_Tick(sender As Object, e As EventArgs) Handles tmrEvents.Tick
        TimeBasedStuff()
    End Sub
#Else
    ' Frontend will have the timer and call TimeBasedStuff() directly.
#End If

End Class