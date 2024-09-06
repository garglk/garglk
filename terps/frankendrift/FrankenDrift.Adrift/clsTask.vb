Public Class clsTask
    Inherits clsItem

    Friend arlCommands As New StringArrayList
    Private bRepeatable As Boolean
    Friend Property ContinueToExecuteLowerPriority As Boolean ' Multiple Matching    
    Friend arlRestrictions As New RestrictionArrayList
    Friend arlActions As New ActionArrayList    
    Private sTriedAgainMessage As String
    Private sDescription As String
    Public Enum BeforeAfterEnum
        Before
        After
    End Enum
    Public eDisplayCompletion As BeforeAfterEnum = BeforeAfterEnum.After
    Public Enum TaskTypeEnum
        General
        Specific
        System
    End Enum
    Public TaskType As TaskTypeEnum
    Private iPriority As Integer
    Private iAutoFillPriority As Integer
    Private oFailOverride As Description
    Public LowPriority As Boolean
    Public Scored As Boolean    
    Public ReplaceDuplicateKey As Boolean
    Public PreventOverriding As Boolean
    Public bSystemTask As Boolean

    Private _RegExs As List(Of System.Text.RegularExpressions.Regex)()
    Public ReadOnly Property RegExs As List(Of System.Text.RegularExpressions.Regex)()
        Get
            If _RegExs Is Nothing Then
                ReDim _RegExs(arlCommands.Count - 1)
                'For Each sCommand As String In arlCommands
                For iCom As Integer = 0 To arlCommands.Count - 1
                    Dim sCommand As String = arlCommands(iCom)
                    _RegExs(iCom) = New List(Of System.Text.RegularExpressions.Regex)
                    For Each regex As System.Text.RegularExpressions.Regex In UserSession.GetRegularExpression(sCommand, "", False)
                        _RegExs(iCom).Add(regex)
                    Next
                Next
            End If

            Return _RegExs
        End Get
    End Property

    ' Want
    ' Specific Text + Parent Text, specific actions, parent actions     - Before*   (text+actions)
    ' Specific Text, specific actions, parent actions                   - Before    (actions)

    ' Specific Text + Parent Text, specific actions                     - Before    (text)
    ' Specific Text, specific actions                                   - Override*
    ' Parent Text + Specific Text, specific actions                     - After     (text)

    ' Specific Text, parent actions, specific actions                   - After     (actions)
    ' Parent Text + Specific Text, parent actions, specific actions     - After     (text+actions)


    ' Old
    ' Parent with text, Specific with text, Execute parent action => Specific text, specific actions, parent actions
    ' Parent with text, Specific without text, Execute parent action => Parent text, specific actions, parent actions
    ' Parent with text, Specific with text, No parent action => Specific text, specific actions
    ' Parent with text, Specific without text, No parent action => Parent text, specific actions

    Public Enum SpecificOverrideTypeEnum
        BeforeTextAndActions
        BeforeActionsOnly
        BeforeTextOnly
        Override
        AfterTextOnly
        AfterActionsOnly
        AfterTextAndActions
    End Enum
    Public SpecificOverrideType As SpecificOverrideTypeEnum = SpecificOverrideTypeEnum.Override

    Private bRunImmediately As Boolean = False
    Public Property RunImmediately() As Boolean
        Get
            Return TaskType = TaskTypeEnum.System AndAlso bRunImmediately
        End Get
        Set(ByVal value As Boolean)
            bRunImmediately = value
        End Set
    End Property

    Public Property LocationTrigger As String  = ""        
    Public Property Completed() As Boolean

    Friend Class Specific
        Public Type As ReferencesType
        Public Multiple As Boolean
        Public Keys As New StringArrayList

        Public Function List() As String

            Dim sList As String = ""

            If Not Keys Is Nothing AndAlso Keys.Count > 0 Then
                For i As Integer = 0 To Keys.Count - 1
                    Select Case Type
                        Case ReferencesType.Direction
                            If Keys(i) <> "" Then sList &= DirectionName(CType([Enum].Parse(GetType(DirectionsEnum), Keys(i)), DirectionsEnum))
                        Case Else
                            sList &= Adventure.GetNameFromKey(Keys(i), False, False)
                    End Select
                    If i + 2 < Keys.Count Then sList &= ", "
                    If i + 2 = Keys.Count Then sList &= " and "
                Next
            End If

            Return sList

        End Function

        Public Function Clone() As Specific
            Dim spec As New Specific

            spec.Type = Type
            spec.Multiple = Multiple
            spec.Keys = Keys.Clone

            Return spec
        End Function

    End Class
    Friend Specifics() As Specific
    Public GeneralKey As String

    Public Property Description() As String
        Get
            If sDescription <> "" Then
                Return sDescription
            ElseIf arlCommands.Count > 0 Then
                Return arlCommands(0)
            Else
                Return Key
            End If
        End Get
        Set(ByVal Value As String)
            sDescription = Value
        End Set
    End Property


    Dim iHasReferences As Integer = -1
    Public ReadOnly Property HasReferences() As Boolean
        Get
            If iHasReferences > -1 Then Return CBool(IIf(iHasReferences = 0, False, True))
            For Each sRef As String In ReferenceNames()
                For Each sCommand As String In Me.arlCommands
                    If sCommand.Contains(sRef) Then
                        iHasReferences = 1
                        Return True
                    End If
                Next
            Next
            iHasReferences = 0
            Return False
        End Get
    End Property

    Friend ReadOnly Property MakeNice() As String
        Get
            ' Makes things like "[get/take] {the} %object% {from {the/a {green}} box}" into "get the %object% from the box"

            Dim iLevel As Integer = 0
            Dim bIgnore() As Boolean
            Dim sText As String = Me.TaskCommand(Me) ' Me.arlCommands(0)

            ReDim bIgnore(0)
            bIgnore(0) = False
            MakeNice = Nothing

            For i As Integer = 0 To sText.Length - 1
                Select Case sText.Substring(i, 1)
                    Case "[", "{"
                        iLevel += 1
                        ReDim Preserve bIgnore(iLevel)
                        bIgnore(iLevel) = bIgnore(iLevel - 1)
                    Case "]", "}"
                        iLevel -= 1
                    Case "/"
                        bIgnore(iLevel) = True
                    Case Else
                        If Not bIgnore(iLevel) Then MakeNice &= sText.Substring(i, 1)
                End Select
            Next

            MakeNice = MakeNice.Replace(" * ", " ").Replace("* ", " ").Replace(" *", " ")

        End Get
    End Property

    Friend Function RefsInCommand(ByVal sCommand As String) As StringArrayList

        Dim arlRefs As New StringArrayList

        For Each sSection As String In sCommand.Split("%"c)
            For Each sRef As String In ReferenceNames()
                If "%" & sSection & "%" = sRef Then
                    arlRefs.Add(sRef)
                    Exit For
                End If
            Next
        Next

        Return arlRefs

    End Function

    Friend ReadOnly Property References() As StringArrayList
        Get         
            Return RefsInCommand(TaskCommand(Me, False))
        End Get
    End Property

    Public Function TaskCommand(ByVal task As clsTask, Optional ByVal bReplaceSpecifics As Boolean = True, Optional ByVal iMatchedTask As Integer = -1) As String

        Dim sTaskCommand As String = ""
        Dim iMatchedTaskCommand As Integer = 0
        If iMatchedTask = -1 Then iMatchedTaskCommand = UserSession.iMatchedTaskCommand

        Select Case task.TaskType
            Case TaskTypeEnum.General
                Dim iRefsCount As Integer = 0
                If task.arlCommands.Count = 0 Then
                    ErrMsg("Error, general task """ & sDescription & """ has no commands!")
                    Return ""
                End If
                If task.arlCommands.Count > iMatchedTaskCommand Then
                    sTaskCommand = task.arlCommands(iMatchedTaskCommand)
                Else
                    sTaskCommand = task.arlCommands(0)
                End If
                If RefsInCommand(sTaskCommand).Count > iRefsCount Then ' Need to make sure we're checking against the command we matched on
                    iRefsCount = RefsInCommand(sTaskCommand).Count
                End If
                ' Some v4 games may not have same no. of refs on each line, so if any other lines have more, use that instead
                For Each sCommand As String In task.arlCommands
                    If RefsInCommand(sCommand).Count > iRefsCount Then
                        iRefsCount = RefsInCommand(sCommand).Count
                        sTaskCommand = sCommand
                    End If
                Next

            Case TaskTypeEnum.Specific
                sTaskCommand = TaskCommand(Adventure.htblTasks(task.GeneralKey), bReplaceSpecifics)
                If bReplaceSpecifics Then
                    ' Replace any Specifics from this key
                    For iSpec As Integer = 0 To Specifics.Length - 1
                        If Specifics(iSpec).Keys.Count = 0 OrElse (Specifics(iSpec).Keys.Count = 1 AndAlso Specifics(iSpec).Keys(0) = "") Then
                            ' Allow, as it's passing thru the parent as a reference
                        Else
                            ' Replace the parent task %object% with our specific key
                            sTaskCommand = sTaskCommand.Replace(Me.References(iSpec), Specifics(iSpec).List)
                        End If
                    Next
                End If
            Case TaskTypeEnum.System
                sTaskCommand = ""
        End Select

        Return sTaskCommand

    End Function

    Friend Property CompletionMessage() As Description

    Friend Property FailOverride() As Description
        Get
            Return oFailOverride
        End Get
        Set(ByVal value As Description)
            oFailOverride = value
        End Set
    End Property

    Public Property Priority() As Integer
        Get
            Return iPriority
        End Get
        Set(ByVal Value As Integer)
            iPriority = Value
            ' TODO - rejig all the other priorities to insert this one
        End Set
    End Property

    Public Property AutoFillPriority() As Integer
        Get
            Return iAutoFillPriority
        End Get
        Set(ByVal value As Integer)
            iAutoFillPriority = value
        End Set
    End Property

    Public Property TriedAgainMessage() As String
        Get
            Return sTriedAgainMessage
        End Get
        Set(ByVal Value As String)
            sTriedAgainMessage = Value
        End Set
    End Property

    Public Property Repeatable() As Boolean
        Get
            Return bRepeatable
        End Get
        Set(ByVal Value As Boolean)
            bRepeatable = Value
        End Set
    End Property

    Public Property AggregateOutput As Boolean = True

    Public Sub New()
        CompletionMessage = New Description
        oFailOverride = New Description

        ContinueToExecuteLowerPriority = False
        Me.iAutoFillPriority = 10
    End Sub

    Private Class TaskPrioritySortComparer
        Implements System.Collections.Generic.IComparer(Of String)

        Public Function Compare(x As String, y As String) As Integer Implements System.Collections.Generic.IComparer(Of String).Compare
            Return Adventure.htblTasks(x).Priority.CompareTo(Adventure.htblTasks(y).Priority)
        End Function
    End Class

    Friend ReadOnly Property Children(Optional ByVal bIncludeCompleted As Boolean = False) As StringArrayList
        Get
            Dim sal As New StringArrayList

            For Each tas As clsTask In Adventure.Tasks(clsAdventure.TasksListEnum.SpecificTasks).Values
                If tas.GeneralKey = Me.Key AndAlso (bIncludeCompleted OrElse Not tas.Completed OrElse tas.Repeatable) Then sal.Add(tas.Key)
            Next
            sal.Sort(New TaskPrioritySortComparer)

            Return sal
        End Get
    End Property

    Public ReadOnly Property Parent() As String
        Get
            Return Me.GeneralKey
        End Get
    End Property

    Public Function IsCompleteable() As Boolean
        Return UserSession.PassRestrictions(Me.arlRestrictions, True, Me)
        Return True ' Hmmm...
    End Function

    Friend NewReferences() As clsNewReference
    Friend NewReferencesWorking() As clsNewReference

    Friend Function CopyNewRefs(ByVal OriginalRefs() As clsNewReference) As clsNewReference()

        If OriginalRefs Is Nothing Then Return Nothing

        Dim NewRefCopy(OriginalRefs.Length - 1) As clsNewReference

        For iRef As Integer = 0 To OriginalRefs.Length - 1
            If OriginalRefs(iRef) IsNot Nothing Then
                Dim nr As New clsNewReference(OriginalRefs(iRef).ReferenceType)
                For iItem As Integer = 0 To OriginalRefs(iRef).Items.Count - 1
                    Dim itm As New clsSingleItem
                    itm.MatchingPossibilities = OriginalRefs(iRef).Items(iItem).MatchingPossibilities.Clone
                    itm.bExplicitlyMentioned = OriginalRefs(iRef).Items(iItem).bExplicitlyMentioned
                    itm.sCommandReference = OriginalRefs(iRef).Items(iItem).sCommandReference
                    nr.Items.Add(itm)
                Next
                nr.Index = OriginalRefs(iRef).Index
                nr.ReferenceMatch = OriginalRefs(iRef).ReferenceMatch
                NewRefCopy(iRef) = nr
            End If
        Next
        Return NewRefCopy
    End Function

    Public Function HasObjectExistRestriction() As Boolean
        Static bChecked As Boolean = False
        Static bResult As Boolean = False

        If Not bChecked Then
            For Each rest As clsRestriction In Me.arlRestrictions
                If rest.eType = clsRestriction.RestrictionTypeEnum.Object AndAlso rest.eObject = clsRestriction.ObjectEnum.Exist Then
                    bResult = True
                    Exit For
                End If
            Next
            bChecked = True
            Return bResult
        Else
            Return bResult
        End If
    End Function

    Public Function HasCharacterExistRestriction() As Boolean
        Static bChecked As Boolean = False
        Static bResult As Boolean = False

        If Not bChecked Then
            For Each rest As clsRestriction In Me.arlRestrictions
                If rest.eType = clsRestriction.RestrictionTypeEnum.Character AndAlso rest.eCharacter = clsRestriction.CharacterEnum.Exist Then
                    bResult = True
                    Exit For
                End If
            Next
            bChecked = True
            Return bResult
        Else
            Return bResult
        End If
    End Function

    Public Function HasLocationExistRestriction() As Boolean
        Static bChecked As Boolean = False
        Static bResult As Boolean = False

        If Not bChecked Then
            For Each rest As clsRestriction In Me.arlRestrictions
                If rest.eType = clsRestriction.RestrictionTypeEnum.Location AndAlso rest.eLocation = clsRestriction.LocationEnum.Exist Then
                    bResult = True
                    Exit For
                End If
            Next
            bChecked = True
            Return bResult
        Else
            Return bResult
        End If
    End Function

    Public Overrides ReadOnly Property Clone() As clsItem
        Get
            Dim tas As clsTask = CType(Me.MemberwiseClone, clsTask)
            tas.arlCommands = tas.arlCommands.Clone
            tas.arlRestrictions = tas.arlRestrictions.Copy
            tas.arlActions = tas.arlActions.Copy
            If Specifics IsNot Nothing Then
                ReDim tas.Specifics(Specifics.Length - 1)
                For i As Integer = 0 To tas.Specifics.Length - 1
                    tas.Specifics(i) = Specifics(i).Clone
                Next
            End If
            Return tas
        End Get
    End Property

    Public Overrides ReadOnly Property CommonName() As String
        Get
            Return Description
        End Get
    End Property

    Friend Overrides ReadOnly Property AllDescriptions() As System.Collections.Generic.List(Of SharedModule.Description)
        Get
            Dim all As New Generic.List(Of Description)
            For Each r As clsRestriction In Me.arlRestrictions
                all.Add(r.oMessage)
            Next
            all.Add(Me.CompletionMessage)
            all.Add(Me.FailOverride)
            Return all
        End Get
    End Property

    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object
        Dim iCount As Integer = iReplacements
        iReplacements += MyBase.FindStringInStringProperty(Me.sDescription, sSearchString, sReplace, bFindAll)
        Return iReplacements - iCount
    End Function

    Public Overrides Function ReferencesKey(ByVal sKey As String) As Integer
        Dim iCount As Integer = 0
        iCount += arlRestrictions.ReferencesKey(sKey)
        iCount += arlActions.ReferencesKey(sKey)
        For Each d As Description In AllDescriptions
            iCount += d.ReferencesKey(sKey)
        Next
        If GeneralKey = sKey Then iCount += 1
        Return iCount

    End Function

    Public Overrides Function DeleteKey(ByVal sKey As String) As Boolean
        If Not arlRestrictions.DeleteKey(sKey) Then Return False
        If Not arlActions.DeleteKey(sKey) Then Return False
        For Each d As Description In AllDescriptions
            If Not d.DeleteKey(sKey) Then Return False
        Next
        If GeneralKey = sKey Then GeneralKey = ""
        Return True
    End Function

End Class


Public Class clsRestriction
    Public Enum RestrictionTypeEnum
        Location
        [Object]
        Task
        Character
        Variable
        [Property]
        Direction
        Expression
        Item
    End Enum
    Friend eType As RestrictionTypeEnum

    Public Enum MustEnum
        Must
        MustNot
    End Enum
    Public eMust As MustEnum

    Public sKey1, sKey2 As String

    Friend oMessage As Description

    Public Enum LocationEnum
        HaveBeenSeenByCharacter
        BeInGroup
        HaveProperty
        BeLocation
        Exist
    End Enum
    Public eLocation As LocationEnum

    Public Enum ObjectEnum
        BeAtLocation
        BeHeldByCharacter
        BeWornByCharacter
        BeVisibleToCharacter
        BeInsideObject
        BeOnObject
        BeInState
        BeInGroup
        HaveBeenSeenByCharacter
        BePartOfObject
        BePartOfCharacter
        Exist
        HaveProperty
        BeExactText
        BeHidden
        BeObject
    End Enum
    Public eObject As ObjectEnum

    Public Enum TaskEnum
        Complete
    End Enum
    Public eTask As TaskEnum

    Public Enum CharacterEnum
        BeAlone
        BeAloneWith
        BeAtLocation
        BeCharacter
        BeHoldingObject
        BeInConversationWith
        BeInPosition
        BeInSameLocationAsCharacter
        BeInSameLocationAsObject
        BeInsideObject
        BeLyingOnObject
        BeInGroup
        BeOfGender
        BeOnObject
        BeOnCharacter
        BeStandingOnObject
        BeSittingOnObject
        BeWearingObject
        BeWithinLocationGroup
        Exist
        HaveProperty
        HaveRouteInDirection
        HaveSeenLocation
        HaveSeenObject
        HaveSeenCharacter
        BeVisibleToCharacter
    End Enum
    Public eCharacter As CharacterEnum

    ' Can only be things common to all items
    Public Enum ItemEnum
        BeAtLocation
        BeCharacter
        BeInSameLocationAsCharacter
        BeInSameLocationAsObject
        BeInsideObject
        BeLocation
        BeInGroup
        BeObject
        BeOnCharacter
        BeOnObject
        BeType
        Exist
        HaveProperty
    End Enum
    Public eItem As ItemEnum

    Public Enum VariableEnum
        LessThan
        LessThanOrEqualTo
        EqualTo
        GreaterThanOrEqualTo
        GreaterThan
        Contain
    End Enum
    Public eVariable As VariableEnum

    Public Overrides Function ToString() As String
        Return Summary
    End Function

    Public IntValue As Integer
    Public StringValue As String


    Public Function ReferencesKey(ByVal sKey As String) As Boolean
        Return sKey1 = sKey OrElse sKey2 = sKey
    End Function


    Public ReadOnly Property Summary() As String
        Get
            Dim sSummary As String = "Undefined Restriction"

            Try
                If sKey1 IsNot Nothing OrElse eType = RestrictionTypeEnum.Expression Then
                    Select Case eType
                        Case RestrictionTypeEnum.Location
                            sSummary = Adventure.GetNameFromKey(sKey1, , , True)
                            Select Case eMust
                                Case MustEnum.Must
                                    sSummary &= " must "
                                Case MustEnum.MustNot
                                    sSummary &= " must not "
                            End Select
                            Select Case eLocation
                                Case LocationEnum.HaveBeenSeenByCharacter
                                    sSummary &= "have been seen by " & Adventure.GetNameFromKey(sKey2)
                                Case LocationEnum.BeInGroup
                                    sSummary &= "be in " & Adventure.GetNameFromKey(sKey2)
                                Case LocationEnum.HaveProperty
                                    sSummary &= "have " & Adventure.GetNameFromKey(sKey2)
                                Case LocationEnum.BeLocation
                                    sSummary &= "be location " & Adventure.GetNameFromKey(sKey2)
                                Case LocationEnum.Exist
                                    sSummary &= "exist"
                            End Select

                        Case RestrictionTypeEnum.Object
                            sSummary = Adventure.GetNameFromKey(sKey1, , , True)
                            Select Case eMust
                                Case MustEnum.Must
                                    sSummary &= " must "
                                Case MustEnum.MustNot
                                    sSummary &= " must not "
                            End Select
                            Select Case eObject
                                Case ObjectEnum.BeAtLocation
                                    sSummary &= "be at " & Adventure.GetNameFromKey(sKey2)
                                    sSummary = sSummary.Replace("at " & HIDDEN, HIDDEN)
                                Case ObjectEnum.BeHeldByCharacter
                                    sSummary &= "be held by " & Adventure.GetNameFromKey(sKey2)
                                Case ObjectEnum.BeHidden
                                    sSummary &= "be hidden"
                                Case ObjectEnum.BeInGroup
                                    sSummary &= "be in object " & Adventure.GetNameFromKey(sKey2)
                                Case ObjectEnum.BeInsideObject
                                    sSummary &= ("be inside " & Adventure.GetNameFromKey(sKey2))
                                Case ObjectEnum.BeInState
                                    sSummary &= "be in state '" & sKey2 & "'"
                                Case ObjectEnum.BeOnObject
                                    sSummary &= ("be on " & Adventure.GetNameFromKey(sKey2))
                                Case ObjectEnum.BePartOfCharacter
                                    sSummary &= "be part of " & Adventure.GetNameFromKey(sKey2)
                                Case ObjectEnum.BePartOfObject
                                    sSummary &= ("be part of " & Adventure.GetNameFromKey(sKey2))
                                Case ObjectEnum.HaveBeenSeenByCharacter
                                    sSummary &= "have been seen by " & Adventure.GetNameFromKey(sKey2)
                                Case ObjectEnum.BeVisibleToCharacter
                                    sSummary &= "be visible to " & Adventure.GetNameFromKey(sKey2)
                                Case ObjectEnum.BeWornByCharacter
                                    sSummary &= "be worn by " & Adventure.GetNameFromKey(sKey2)
                                Case ObjectEnum.Exist
                                    sSummary &= "exist"
                                Case ObjectEnum.HaveProperty
                                    sSummary &= "have " & Adventure.GetNameFromKey(sKey2)
                                Case ObjectEnum.BeExactText
                                    sSummary &= "be exact text '" & sKey2 & "'"
                                Case ObjectEnum.BeObject
                                    sSummary &= "be " & Adventure.GetNameFromKey(sKey2)
                            End Select

                        Case RestrictionTypeEnum.Task
                            sSummary = Adventure.GetNameFromKey(sKey1, , , True)
                            Select Case eMust
                                Case MustEnum.Must
                                    sSummary &= " must be complete"
                                Case MustEnum.MustNot
                                    sSummary &= " must not be complete"
                            End Select

                        Case RestrictionTypeEnum.Character
                            sSummary = Adventure.GetNameFromKey(sKey1, , , True)
                            Select Case eMust
                                Case MustEnum.Must
                                    sSummary &= " must "
                                Case MustEnum.MustNot
                                    sSummary &= " must not "
                            End Select
                            Select Case eCharacter
                                Case CharacterEnum.BeAlone
                                    sSummary &= "be alone"
                                Case CharacterEnum.BeAloneWith
                                    sSummary &= "be alone with " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeAtLocation
                                    sSummary &= "be at " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeCharacter
                                    sSummary &= "be " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeInConversationWith
                                    sSummary &= "be in conversation with " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.Exist
                                    sSummary &= "exist"
                                Case CharacterEnum.HaveRouteInDirection
                                    sSummary &= "have a route available "
                                    If sKey2 = ANYDIRECTION Then
                                        sSummary &= "in any direction"
                                    ElseIf sKey2.StartsWith("ReferencedDirection") Then
                                        sSummary &= "to the " & sKey2.Replace("ReferencedDirection", "Referenced Direction")
                                    Else
                                        Select Case EnumParseDirections(sKey2)
                                            Case DirectionsEnum.North, DirectionsEnum.NorthEast, DirectionsEnum.East, DirectionsEnum.SouthEast, DirectionsEnum.South, DirectionsEnum.SouthWest, DirectionsEnum.West, DirectionsEnum.NorthWest
                                                sSummary &= "to the " & DirectionName(EnumParseDirections(sKey2))
                                            Case DirectionsEnum.Up, DirectionsEnum.Down, DirectionsEnum.In, DirectionsEnum.Out
                                                sSummary &= DirectionName(EnumParseDirections(sKey2))
                                        End Select

                                    End If
                                Case CharacterEnum.HaveProperty
                                    sSummary &= "have " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.HaveSeenCharacter
                                    sSummary &= "have seen " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.HaveSeenLocation
                                    sSummary &= "have seen " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.HaveSeenObject
                                    sSummary &= "have seen " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeHoldingObject
                                    sSummary &= "be holding " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeInSameLocationAsCharacter
                                    sSummary &= "be in the same location as " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeInSameLocationAsObject
                                    If sKey2.StartsWith("ReferencedObject") OrElse Adventure.GetTypeFromKeyNice(sKey2) = "Object" Then
                                        sSummary &= "be in the same location as " & Adventure.GetNameFromKey(sKey2)
                                    Else
                                        sSummary &= "be in the same location as any object in " & Adventure.GetNameFromKey(sKey2)
                                    End If
                                Case CharacterEnum.BeLyingOnObject
                                    sSummary &= "be lying on " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeInGroup
                                    sSummary &= "be a member of " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeOfGender
                                    sSummary &= "be of gender " & sKey2
                                Case CharacterEnum.BeSittingOnObject
                                    sSummary &= "be sitting on " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeStandingOnObject
                                    sSummary &= "be standing on " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeWearingObject
                                    sSummary &= "be wearing " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeWithinLocationGroup
                                    sSummary &= "be at a location within " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeInPosition
                                    sSummary &= "be in position " & sKey2
                                Case CharacterEnum.BeInsideObject
                                    sSummary &= "be inside " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeOnObject
                                    sSummary &= "be on " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeOnCharacter
                                    sSummary &= "be on " & Adventure.GetNameFromKey(sKey2)
                                Case CharacterEnum.BeVisibleToCharacter
                                    sSummary &= "be visible to " & Adventure.GetNameFromKey(sKey2)
                            End Select

                        Case RestrictionTypeEnum.Item
                            sSummary = Adventure.GetNameFromKey(sKey1, , , True)
                            Select Case eMust
                                Case MustEnum.Must
                                    sSummary &= " must "
                                Case MustEnum.MustNot
                                    sSummary &= " must not "
                            End Select
                            Select Case eItem
                                Case ItemEnum.BeAtLocation
                                    sSummary &= "be at " & Adventure.GetNameFromKey(sKey2)
                                Case ItemEnum.BeCharacter
                                    sSummary &= "be " & Adventure.GetNameFromKey(sKey2)
                                Case ItemEnum.BeInGroup
                                    sSummary &= "be a member of " & Adventure.GetNameFromKey(sKey2)
                                Case ItemEnum.BeInSameLocationAsCharacter
                                    sSummary &= "be in the same loc as " & Adventure.GetNameFromKey(sKey2)
                                Case ItemEnum.BeInSameLocationAsObject
                                    sSummary &= "be in the same loc as " & Adventure.GetNameFromKey(sKey2)
                                Case ItemEnum.BeInsideObject
                                    sSummary &= "be inside " & Adventure.GetNameFromKey(sKey2)
                                Case ItemEnum.BeLocation
                                    sSummary &= "be " & Adventure.GetNameFromKey(sKey2)
                                Case ItemEnum.BeObject
                                    sSummary &= "be " & Adventure.GetNameFromKey(sKey2)
                                Case ItemEnum.BeOnCharacter
                                    sSummary &= "be on " & Adventure.GetNameFromKey(sKey2)
                                Case ItemEnum.BeOnObject
                                    sSummary &= "be on " & Adventure.GetNameFromKey(sKey2)
                                Case ItemEnum.BeType
                                    sSummary &= "be item type " & sKey2
                                Case ItemEnum.Exist
                                    sSummary &= "exist"
                                Case ItemEnum.HaveProperty
                                    sSummary &= "have " & Adventure.GetNameFromKey(sKey2)
                            End Select

                        Case RestrictionTypeEnum.Variable
                            Dim eVarType As clsVariable.VariableTypeEnum

                            If sKey1.StartsWith("ReferencedNumber") Then
                                sSummary = Adventure.GetNameFromKey(sKey1) & " "
                                eVarType = clsVariable.VariableTypeEnum.Numeric
                            ElseIf sKey1.StartsWith("ReferencedText") Then
                                sSummary = Adventure.GetNameFromKey(sKey1) & " "
                                eVarType = clsVariable.VariableTypeEnum.Text
                            Else
                                If sKey1.StartsWith("Parameter-") Then
                                    sSummary = "Parameter '" & Adventure.GetNameFromKey(sKey1) & "' "
                                Else
                                    Dim var As clsVariable = Adventure.htblVariables(sKey1)
                                    eVarType = var.Type
                                    sSummary = "Variable '" & var.Name
                                    If sKey2 <> "" Then
                                        If Adventure.htblVariables.ContainsKey(sKey2) Then
                                            sSummary &= "[%" & Adventure.htblVariables(sKey2).Name & "%]"
                                        Else
                                            sSummary &= "[" & sKey2 & "]"
                                        End If
                                    End If
                                    sSummary &= "' "
                                End If
                            End If

                            Select Case eMust
                                Case MustEnum.Must
                                    sSummary &= "must "
                                Case MustEnum.MustNot
                                    sSummary &= "must not "
                            End Select

                            If eVarType = clsVariable.VariableTypeEnum.Numeric Then
                                sSummary &= "be "

                                Select Case eVariable
                                    Case VariableEnum.EqualTo
                                        sSummary &= "equal to "
                                    Case VariableEnum.GreaterThan
                                        sSummary &= "greater than "
                                    Case VariableEnum.GreaterThanOrEqualTo
                                        sSummary &= "greater than or equal to "
                                    Case VariableEnum.LessThan
                                        sSummary &= "less than "
                                    Case VariableEnum.LessThanOrEqualTo
                                        sSummary &= "less than or equal to "
                                End Select
                            Else
                                Select Case eVariable
                                    Case VariableEnum.EqualTo
                                        sSummary &= "be equal to "
                                    Case VariableEnum.Contain
                                        sSummary &= "contain "
                                End Select
                            End If


                            If IntValue = Integer.MinValue Then
                                sSummary &= Adventure.GetNameFromKey(StringValue)
                            Else
                                If eVarType = clsVariable.VariableTypeEnum.Numeric Then
                                    If StringValue <> "" AndAlso StringValue <> IntValue.ToString Then
                                        sSummary &= StringValue ' Must be an expression resulting in an integer
                                    Else
                                        sSummary &= IntValue
                                    End If
                                Else
                                    sSummary &= "'" & StringValue & "'"
                                End If
                            End If

                        Case RestrictionTypeEnum.Property
                            Dim p As clsProperty = Nothing
                            If Adventure.htblAllProperties.TryGetValue(sKey1, p) Then
                                sSummary = Adventure.GetNameFromKey(sKey1, , , True) & " for " & Adventure.GetNameFromKey(sKey2)
                                Select Case eMust
                                    Case MustEnum.Must
                                        sSummary &= " must be "
                                    Case MustEnum.MustNot
                                        sSummary &= " must not be "
                                End Select
                                Select Case p.Type
                                    Case clsProperty.PropertyTypeEnum.CharacterKey, clsProperty.PropertyTypeEnum.LocationGroupKey, clsProperty.PropertyTypeEnum.LocationKey, clsProperty.PropertyTypeEnum.ObjectKey
                                        sSummary &= "'" & Adventure.GetNameFromKey(StringValue) & "'"
                                    Case clsProperty.PropertyTypeEnum.SelectionOnly
                                    Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.ValueList
                                        Select Case CType(IntValue, VariableEnum)
                                            Case VariableEnum.LessThan
                                                sSummary &= "< "
                                            Case VariableEnum.LessThanOrEqualTo
                                                sSummary &= "<= "
                                            Case VariableEnum.EqualTo
                                                sSummary &= "= "
                                            Case VariableEnum.GreaterThanOrEqualTo
                                                sSummary &= ">= "
                                            Case VariableEnum.GreaterThan
                                                sSummary &= "> "
                                        End Select

                                        Dim bSetVar As Boolean = False
                                        If p.Type = clsProperty.PropertyTypeEnum.ValueList AndAlso IsNumeric(StringValue) Then
                                            For Each sValue As String In p.ValueList.Keys
                                                If p.ValueList(sValue) = SafeInt(StringValue) Then
                                                    sSummary &= "'" & sValue & "'"
                                                    bSetVar = True
                                                    Exit For
                                                End If
                                            Next
                                        End If
                                        If Not bSetVar Then sSummary &= "'" & StringValue & "'"
                                    Case clsProperty.PropertyTypeEnum.StateList, clsProperty.PropertyTypeEnum.Text
                                        sSummary &= "'" & StringValue & "'"
                                End Select
                            End If

                        Case RestrictionTypeEnum.Direction
                            sSummary = "Referenced Direction "
                            Select Case eMust
                                Case MustEnum.Must
                                    sSummary &= "must be "
                                Case MustEnum.MustNot
                                    sSummary &= "must not be "
                            End Select
                            sSummary &= DirectionName(EnumParseDirections(sKey1))

                        Case RestrictionTypeEnum.Expression
                            sSummary = StringValue

                    End Select
                End If

            Catch
                ErrMsg("Error producing restriction summary")
            End Try

            Return sSummary
        End Get
    End Property


    Public Function Copy() As clsRestriction
        Dim rest As New clsRestriction

        rest.eCharacter = Me.eCharacter
        rest.eType = Me.eType
        rest.eLocation = Me.eLocation
        rest.eMust = Me.eMust
        rest.eObject = Me.eObject
        rest.eTask = Me.eTask
        rest.eVariable = Me.eVariable
        rest.eItem = Me.eItem
        rest.sKey1 = Me.sKey1
        rest.sKey2 = Me.sKey2
        rest.IntValue = Me.IntValue
        rest.StringValue = Me.StringValue
        rest.oMessage = Me.oMessage.Copy

        Return rest
    End Function

    Public Sub New()
        oMessage = New Description
    End Sub

End Class



Public Class clsAction

    Public Enum ItemEnum
        ' Objects
        MoveObject
        AddObjectToGroup
        RemoveObjectFromGroup

        ' Characters
        MoveCharacter
        AddCharacterToGroup
        RemoveCharacterFromGroup

        ' Variables
        SetVariable
        IncreaseVariable
        DecreaseVariable

        SetProperties
        SetTasks
        EndGame
        Conversation

        ' Locations
        AddLocationToGroup
        RemoveLocationFromGroup

        Time
    End Enum
    Public eItem As ItemEnum

    Public sKey1, sKey2 As String
    Public sPropertyValue As String

    Public Enum MoveObjectWhatEnum
        [Object]
        EverythingHeldBy
        EverythingWornBy
        EverythingInside
        EverythingOn
        EverythingWithProperty
        EverythingInGroup
        EverythingAtLocation
    End Enum
    Public eMoveObjectWhat As MoveObjectWhatEnum

    Public Enum MoveObjectToEnum
        ' Static or Dynamic Moves
        ToLocation
        ToSameLocationAs ' Object or Character
        ToLocationGroup ' If static, moves to all locations.  If dynamic, moves to random location
        '
        ' Dynamic Moves
        '
        InsideObject
        OntoObject
        ToCarriedBy
        ToWornBy
        '
        ' Static Moves
        ToPartOfCharacter
        ToPartOfObject
        '
        ' To/From Group
        ToGroup
        FromGroup
    End Enum
    Public eMoveObjectTo As MoveObjectToEnum

    Public Enum MoveCharacterWhoEnum
        Character
        EveryoneInside
        EveryoneOn
        EveryoneWithProperty
        EveryoneInGroup
        EveryoneAtLocation
    End Enum
    Public eMoveCharacterWho As MoveCharacterWhoEnum

    Public Enum MoveCharacterToEnum
        InDirection
        ToLocation
        ToLocationGroup
        ToSameLocationAs ' Object or Character
        ToStandingOn
        ToSittingOn
        ToLyingOn
        ToSwitchWith
        InsideObject
        OntoCharacter
        ToParentLocation
        ToGroup
        FromGroup
    End Enum
    Public eMoveCharacterTo As MoveCharacterToEnum

    Public Enum MoveLocationWhatEnum
        Location
        LocationOf
        EverywhereInGroup
        EverywhereWithProperty
    End Enum
    Public eMoveLocationWhat As MoveLocationWhatEnum

    Public Enum MoveLocationToEnum
        ' To/From Group
        ToGroup
        FromGroup
    End Enum
    Public eMoveLocationTo As MoveLocationToEnum

    Public Enum ObjectStatusEnum
        DunnoYet
    End Enum
    Public eObjectStatus As ObjectStatusEnum

    Public Enum VariablesEnum
        Assignment
        [Loop]
    End Enum
    Public eVariables As VariablesEnum

    Public Enum SetTasksEnum
        Execute
        Unset
    End Enum
    Public eSetTasks As SetTasksEnum

    Public Enum EndGameEnum
        Win
        Lose
        Neutral
        Running
    End Enum
    Public eEndgame As EndGameEnum

    <Flags> _
    Friend Enum ConversationEnum
        Greet = 1
        Ask = 2
        Tell = 4
        Command = 8
        Farewell = 16
        EnterConversation = 32
        LeaveConversation = 64
    End Enum
    Friend eConversation As ConversationEnum

    Public IntValue As Integer
    Public StringValue As String

    Public Function Copy() As clsAction

        Dim act As New clsAction

        act.eendgame = Me.eendgame
        act.eItem = Me.eItem
        act.eMoveObjectWhat = Me.eMoveObjectWhat
        act.eMoveObjectTo = Me.eMoveObjectTo
        act.eMoveCharacterWho = Me.eMoveCharacterWho
        act.eMoveCharacterTo = Me.eMoveCharacterTo
        act.eMoveLocationWhat = Me.eMoveLocationWhat
        act.eMoveLocationTo = Me.eMoveLocationTo
        act.sPropertyValue = Me.sPropertyValue
        act.eObjectStatus = Me.eObjectStatus
        act.eSetTasks = Me.eSetTasks
        act.eVariables = Me.eVariables
        act.eConversation = Me.eConversation
        act.IntValue = Me.IntValue
        act.sKey1 = Me.sKey1
        act.sKey2 = Me.sKey2
        act.StringValue = Me.StringValue

        Return act

    End Function

    Public Overrides Function ToString() As String
        Return Summary
    End Function

    Public Function ReferencesKey(ByVal sKey As String) As Boolean
        Return sKey1 = sKey OrElse sKey2 = sKey
    End Function

    Public ReadOnly Property Summary() As String
        Get
            Dim sSummary As String = "Undefined Action"

            Try
                Select Case eItem
                    Case ItemEnum.MoveObject, ItemEnum.AddObjectToGroup, ItemEnum.RemoveObjectFromGroup

                        Select Case eItem
                            Case ItemEnum.MoveObject
                                sSummary = "Move "
                            Case ItemEnum.AddObjectToGroup
                                sSummary = "Add "
                            Case ItemEnum.RemoveObjectFromGroup
                                sSummary = "Remove "
                            Case Else
                                ErrMsg("eItem not specified")
                        End Select

                        Dim bDynamic As Boolean = True
                        Select Case eMoveObjectWhat
                            Case MoveObjectWhatEnum.Object
                                If Not sKey1.StartsWith("ReferencedObject") Then bDynamic = Not Adventure.htblObjects(sKey1).IsStatic

                            Case MoveObjectWhatEnum.EverythingHeldBy
                                sSummary &= "everything held by "

                            Case MoveObjectWhatEnum.EverythingAtLocation
                                sSummary &= "everything at "

                            Case MoveObjectWhatEnum.EverythingInGroup
                                sSummary &= "everything in "

                            Case MoveObjectWhatEnum.EverythingInside
                                sSummary &= "everything inside "

                            Case MoveObjectWhatEnum.EverythingOn
                                sSummary &= "everything on "

                            Case MoveObjectWhatEnum.EverythingWithProperty
                                sSummary &= "everything with " & Adventure.GetNameFromKey(sKey1)
                                If sPropertyValue <> "" Then
                                    Dim prop As clsProperty = Adventure.htblObjectProperties(sKey1)
                                    If prop IsNot Nothing Then
                                        sSummary &= " where value is "
                                        Select Case prop.Type
                                            Case clsProperty.PropertyTypeEnum.CharacterKey, clsProperty.PropertyTypeEnum.LocationGroupKey, clsProperty.PropertyTypeEnum.LocationKey, clsProperty.PropertyTypeEnum.ObjectKey
                                                sSummary &= Adventure.GetNameFromKey(sPropertyValue)
                                            Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.StateList, clsProperty.PropertyTypeEnum.Text
                                                sSummary &= "'" & sPropertyValue & "'"
                                            Case clsProperty.PropertyTypeEnum.SelectionOnly
                                                ' N/A
                                        End Select
                                    End If
                                End If

                            Case MoveObjectWhatEnum.EverythingWornBy
                                sSummary &= "everything worn by "

                            Case Else
                                ErrMsg("eMoveObjectWhat not specified")
                        End Select
                        If eMoveObjectWhat <> MoveObjectWhatEnum.EverythingWithProperty Then sSummary &= Adventure.GetNameFromKey(sKey1)

                        Select Case eMoveObjectTo
                            Case MoveObjectToEnum.InsideObject
                                sSummary &= " inside " & Adventure.GetNameFromKey(sKey2)
                            Case MoveObjectToEnum.OntoObject
                                sSummary &= " onto " & Adventure.GetNameFromKey(sKey2)
                            Case MoveObjectToEnum.ToCarriedBy
                                sSummary &= " to held by " & Adventure.GetNameFromKey(sKey2)
                            Case MoveObjectToEnum.ToLocation
                                If sKey2 = "Hidden" Then
                                    sSummary &= " to Hidden"
                                Else
                                    sSummary &= " to " & Adventure.GetNameFromKey(sKey2)
                                End If
                            Case MoveObjectToEnum.ToLocationGroup
                                If bDynamic Then
                                    sSummary &= " to somewhere in location " & Adventure.GetNameFromKey(sKey2)
                                Else
                                    sSummary &= " to everywhere in location " & Adventure.GetNameFromKey(sKey2)
                                End If
                            Case MoveObjectToEnum.ToPartOfCharacter
                                sSummary &= " to part of " & Adventure.GetNameFromKey(sKey2)
                            Case MoveObjectToEnum.ToPartOfObject
                                sSummary &= " to part of " & Adventure.GetNameFromKey(sKey2)
                            Case MoveObjectToEnum.ToSameLocationAs
                                sSummary &= " to same location as " & Adventure.GetNameFromKey(sKey2)
                            Case MoveObjectToEnum.ToWornBy
                                sSummary &= " to worn by " & Adventure.GetNameFromKey(sKey2)
                            Case MoveObjectToEnum.ToGroup
                                sSummary &= " to " & Adventure.GetNameFromKey(sKey2)
                            Case MoveObjectToEnum.FromGroup
                                sSummary &= " from " & Adventure.GetNameFromKey(sKey2)
                        End Select

                    Case ItemEnum.MoveCharacter, ItemEnum.AddCharacterToGroup, ItemEnum.RemoveCharacterFromGroup

                        Select Case eItem
                            Case ItemEnum.MoveCharacter
                                sSummary = "Move "
                            Case ItemEnum.AddCharacterToGroup
                                sSummary = "Add "
                            Case ItemEnum.RemoveCharacterFromGroup
                                sSummary = "Remove "
                            Case Else
                                ErrMsg("eItem not specified")
                        End Select

                        Select Case eMoveCharacterWho
                            Case MoveCharacterWhoEnum.Character
                                '
                            Case MoveCharacterWhoEnum.EveryoneAtLocation
                                sSummary &= "everyone at "
                            Case MoveCharacterWhoEnum.EveryoneInGroup
                                sSummary &= "everyone in "
                            Case MoveCharacterWhoEnum.EveryoneInside
                                sSummary &= "everyone inside "
                            Case MoveCharacterWhoEnum.EveryoneOn
                                sSummary &= "everyone on "
                            Case MoveCharacterWhoEnum.EveryoneWithProperty
                                sSummary &= "everyone with " & Adventure.GetNameFromKey(sKey1)
                                If sPropertyValue <> "" Then
                                    Dim prop As clsProperty = Adventure.htblCharacterProperties(sKey1)
                                    If prop IsNot Nothing Then
                                        sSummary &= " where value is "
                                        Select Case prop.Type
                                            Case clsProperty.PropertyTypeEnum.CharacterKey, clsProperty.PropertyTypeEnum.LocationGroupKey, clsProperty.PropertyTypeEnum.LocationKey, clsProperty.PropertyTypeEnum.ObjectKey
                                                sSummary &= Adventure.GetNameFromKey(sPropertyValue)
                                            Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.StateList, clsProperty.PropertyTypeEnum.Text
                                                sSummary &= "'" & sPropertyValue & "'"
                                            Case clsProperty.PropertyTypeEnum.SelectionOnly
                                                ' N/A
                                        End Select
                                    End If
                                End If
                            Case Else
                                ErrMsg("eMoveCharacterWho not specified")
                        End Select
                        If eMoveCharacterWho <> MoveCharacterWhoEnum.EveryoneWithProperty Then sSummary &= Adventure.GetNameFromKey(sKey1)

                        Select Case eMoveCharacterTo
                            Case MoveCharacterToEnum.InDirection
                                sSummary &= " in direction"
                                If sKey2.StartsWith("ReferencedDirection") Then
                                    sSummary &= sKey2.Replace("ReferencedDirection", " Referenced Direction ")
                                Else
                                    sSummary &= " " & DirectionName(EnumParseDirections(sKey2))
                                End If
                            Case MoveCharacterToEnum.ToLocation
                                sSummary &= " to " & Adventure.GetNameFromKey(sKey2)
                            Case MoveCharacterToEnum.ToLocationGroup
                                sSummary &= " to " & Adventure.GetNameFromKey(sKey2)
                            Case MoveCharacterToEnum.ToSameLocationAs
                                sSummary &= " to same location as " & Adventure.GetNameFromKey(sKey2)
                            Case MoveCharacterToEnum.ToStandingOn
                                sSummary &= " to standing on " & Adventure.GetNameFromKey(sKey2)
                            Case MoveCharacterToEnum.ToSittingOn
                                sSummary &= " to sitting on " & Adventure.GetNameFromKey(sKey2)
                            Case MoveCharacterToEnum.ToSwitchWith
                                sSummary &= " to switch places with " & Adventure.GetNameFromKey(sKey2)
                            Case MoveCharacterToEnum.ToLyingOn
                                sSummary &= " to lying on " & Adventure.GetNameFromKey(sKey2)
                            Case MoveCharacterToEnum.InsideObject
                                sSummary &= " inside " & Adventure.GetNameFromKey(sKey2)
                            Case MoveCharacterToEnum.OntoCharacter
                                sSummary &= " onto " & Adventure.GetNameFromKey(sKey2)
                            Case MoveCharacterToEnum.ToParentLocation
                                sSummary &= " to parent location"
                            Case MoveCharacterToEnum.ToGroup
                                sSummary &= " to " & Adventure.GetNameFromKey(sKey2)
                            Case MoveCharacterToEnum.FromGroup
                                sSummary &= " from " & Adventure.GetNameFromKey(sKey2)
                        End Select

                    Case ItemEnum.AddLocationToGroup, ItemEnum.RemoveLocationFromGroup

                        Select Case eItem
                            Case ItemEnum.AddLocationToGroup
                                sSummary = "Add "
                            Case ItemEnum.RemoveLocationFromGroup
                                sSummary = "Remove "
                            Case Else
                                ErrMsg("eItem not specified")
                        End Select

                        Select Case eMoveLocationWhat
                            Case MoveLocationWhatEnum.Location
                                '
                            Case MoveLocationWhatEnum.LocationOf
                                sSummary &= "location of "
                            Case MoveLocationWhatEnum.EverywhereInGroup
                                sSummary &= "everywhere in "
                            Case MoveLocationWhatEnum.EverywhereWithProperty
                                sSummary &= "everywhere with " & Adventure.GetNameFromKey(sKey1)
                                If sPropertyValue <> "" Then
                                    Dim prop As clsProperty = Adventure.htblLocationProperties(sKey1)
                                    If prop IsNot Nothing Then
                                        sSummary &= " where value is "
                                        Select Case prop.Type
                                            Case clsProperty.PropertyTypeEnum.CharacterKey, clsProperty.PropertyTypeEnum.LocationGroupKey, clsProperty.PropertyTypeEnum.LocationKey, clsProperty.PropertyTypeEnum.ObjectKey
                                                sSummary &= Adventure.GetNameFromKey(sPropertyValue)
                                            Case clsProperty.PropertyTypeEnum.Integer, clsProperty.PropertyTypeEnum.StateList, clsProperty.PropertyTypeEnum.Text
                                                sSummary &= "'" & sPropertyValue & "'"
                                            Case clsProperty.PropertyTypeEnum.SelectionOnly
                                                ' N/A
                                        End Select
                                    End If
                                End If
                            Case Else
                                ErrMsg("eMoveLocationWhat not specified")
                        End Select
                        If eMoveLocationWhat <> MoveLocationWhatEnum.EverywhereWithProperty Then sSummary &= Adventure.GetNameFromKey(sKey1)

                        Select Case eMoveLocationTo
                            Case MoveLocationToEnum.ToGroup
                                sSummary &= " to " & Adventure.GetNameFromKey(sKey2)
                            Case MoveLocationToEnum.FromGroup
                                sSummary &= " from " & Adventure.GetNameFromKey(sKey2)
                        End Select

                    Case ItemEnum.SetProperties
                        Dim p As clsProperty = Nothing
                        If Adventure.htblAllProperties.TryGetValue(sKey2, p) Then
                            sSummary = "Set property '" & p.Description & "' of " & Adventure.GetNameFromKey(sKey1) & " to "
                            Dim bSetVar As Boolean = False
                            If sPropertyValue.Length > 2 AndAlso sPropertyValue.StartsWith("%") AndAlso sPropertyValue.EndsWith("%") Then 'AndAlso Adventure.htblVariables.ContainsKey(value.Substring(1, value.Length - 2)) Then
                                For Each v As clsVariable In Adventure.htblVariables.Values
                                    If sPropertyValue = "%" & v.Name & "%" Then
                                        sSummary &= Adventure.GetNameFromKey(v.Key)
                                        bSetVar = True
                                        Exit For
                                    End If
                                Next
                            End If
                            If Not bSetVar AndAlso p.Type = clsProperty.PropertyTypeEnum.ValueList AndAlso IsNumeric(sPropertyValue) Then
                                For Each sValue As String In p.ValueList.Keys
                                    If p.ValueList(sValue) = SafeInt(sPropertyValue) Then
                                        sSummary &= "'" & sValue & "'"
                                        bSetVar = True
                                        Exit For
                                    End If
                                Next
                            End If
                            If Not bSetVar AndAlso sPropertyValue.StartsWith("Referenced") Then
                                sSummary &= Adventure.GetNameFromKey(sPropertyValue)
                                bSetVar = True
                            End If
                            If Not bSetVar Then sSummary &= "'" & sPropertyValue & "'"
                        End If

                    Case ItemEnum.SetVariable, ItemEnum.IncreaseVariable, ItemEnum.DecreaseVariable
                        Dim var As clsVariable = Adventure.htblVariables(sKey1)
                        Select Case Me.eVariables
                            Case VariablesEnum.Assignment
                                Select Case eItem
                                    Case ItemEnum.SetVariable
                                        sSummary = "Set"
                                    Case ItemEnum.IncreaseVariable
                                        sSummary = "Increase"
                                    Case ItemEnum.DecreaseVariable
                                        sSummary = "Decrease"
                                End Select
                                sSummary &= " variable " & var.Name
                                If Adventure.htblVariables(sKey1).Length > 1 Then
                                    If IsNumeric(sKey2) Then
                                        sSummary &= "[" & sKey2 & "]"
                                    ElseIf sKey2.StartsWith("ReferencedNumber") Then
                                        sSummary &= Adventure.GetNameFromKey(sKey2, False) 'We already get brackets in the ref name, so don't double them up
                                    Else
                                        sSummary &= "[%" & Adventure.htblVariables(sKey2).Name & "%]"
                                    End If
                                End If
                                Select Case eItem
                                    Case ItemEnum.SetVariable
                                        sSummary &= " to "
                                    Case ItemEnum.IncreaseVariable, ItemEnum.DecreaseVariable
                                        sSummary &= " by "
                                End Select
                                If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                                    sSummary &= "'" & StringValue & "'"
                                Else
                                    sSummary &= "'" & StringValue & "'" ' already has quotes around it - oh really, not if it's an expression
                                End If
                            Case VariablesEnum.Loop
                                sSummary = "FOR %Loop% = " & IntValue & " TO " & CInt(Val(sKey2)) & " : SET " & var.Name & "[%Loop%] = " & StringValue & " : NEXT %Loop%"
                        End Select

                    Case ItemEnum.SetTasks
                        If sPropertyValue <> "" Then
                            sSummary = "FOR %Loop% = " & IntValue & " TO " & CInt(sPropertyValue) & " : "
                        Else
                            sSummary = ""
                        End If
                        Select Case Me.eSetTasks
                            Case SetTasksEnum.Execute
                                sSummary &= "Execute " & Adventure.GetNameFromKey(sKey1)
                                If StringValue <> "" Then sSummary &= " (" & StringValue.Replace("|", ", ") & ")"
                            Case SetTasksEnum.Unset
                                sSummary &= "Unset " & Adventure.GetNameFromKey(sKey1)
                        End Select
                        If sPropertyValue <> "" Then
                            sSummary &= " : NEXT %Loop%"
                        End If
                    Case ItemEnum.Time
                        sSummary = "Skip " & StringValue & " turns"

                    Case ItemEnum.EndGame
                        Select Case Me.eEndgame
                            Case EndGameEnum.Win
                                sSummary = "End game in Victory"
                            Case EndGameEnum.Lose
                                sSummary = "End game in Defeat"
                            Case EndGameEnum.Neutral
                                sSummary = "End game"
                        End Select

                    Case ItemEnum.Conversation
                        Select Case Me.eConversation
                            Case ConversationEnum.Greet
                                sSummary = "Greet " & Adventure.GetNameFromKey(sKey1) & IIf(StringValue <> "", " with '" & StringValue & "'", "").ToString
                            Case ConversationEnum.Ask
                                sSummary = "Ask " & Adventure.GetNameFromKey(sKey1) & " about '" & StringValue & "'"
                            Case ConversationEnum.Tell
                                sSummary = "Tell " & Adventure.GetNameFromKey(sKey1) & " about '" & StringValue & "'"
                            Case ConversationEnum.Command
                                sSummary = "Say '" & StringValue & "' to " & Adventure.GetNameFromKey(sKey1)
                            Case ConversationEnum.EnterConversation
                                sSummary = "Enter conversation with " & Adventure.GetNameFromKey(sKey1)
                            Case ConversationEnum.LeaveConversation
                                sSummary = "Leave conversation with " & Adventure.GetNameFromKey(sKey1)
                        End Select
                End Select
            Catch
                sSummary = "Bad Action Definition"
            End Try
            Return sSummary
        End Get
    End Property

End Class