Imports System.IO
Imports System.Collections.Generic


' Gives us the same functionality as a stack, but restricts it to 100
Public Class MyStack
    Private Const MAXLENGTH As Integer = 100

    Private States(MAXLENGTH) As clsGameState
    Private iStart As Integer = -1
    Private iEnd As Integer = -1

    Friend Sub Push(ByVal state As clsGameState)
        If iStart > iEnd Then iStart += 1
        If iStart > MAXLENGTH Then iStart = 0
        iEnd += 1

        If iEnd > MAXLENGTH Then
            iEnd = 0
            If iStart = 0 Then iStart = 1
        End If
        If iStart = -1 Then iStart = 0

        States(iEnd) = state
    End Sub

    Friend Function Count() As Integer
        If iEnd >= iStart Then
            If iStart = -1 AndAlso iEnd = -1 Then Return 0
            Return iEnd - iStart + 1
        Else
            Return MAXLENGTH + 1
        End If
    End Function

    Friend Function Peek() As clsGameState
        If Count() = 0 Then Return Nothing
        Return States(iEnd)
    End Function

    Friend Function Pop() As clsGameState

        Dim state As clsGameState = Peek()
        If state IsNot Nothing Then
            iEnd -= 1
            If iEnd < 0 Then
                iEnd = MAXLENGTH
            End If
        End If
        Return state

    End Function


    Friend Sub Clear()
        iStart = -1
        iEnd = -1
    End Sub

End Class

Public Class StateStack
    Inherits MyStack

    Friend Shadows Sub Push(ByVal state As clsGameState)
        If stateCurrent IsNot Nothing Then stateLast = stateCurrent
        MyBase.Push(state)
        Debug.WriteLine("Pushed (" & MyBase.Count & " on stack)")
        stateCurrent = state
    End Sub

    Shadows Sub Pop()
        RestoreState(CType(MyBase.Pop, clsGameState))
        Debug.WriteLine("Popped (" & MyBase.Count & " on stack)")
    End Sub

    Private Sub SaveDisplayOnce(ByVal AllDescriptions As List(Of Description), ByVal htblStore As Dictionary(Of String, Boolean))

        Dim iDesc As Integer = 0
        For Each d As Description In AllDescriptions
            iDesc += 1
            Dim iSing As Integer = 0
            For Each sd As SingleDescription In d
                iSing += 1
                If sd.DisplayOnce AndAlso sd.Displayed Then
                    Dim sKey As String = iDesc & "-" & iSing
                    htblStore.Add(sKey, True)
                End If
            Next
        Next
    End Sub

    ' Get the current game state, and store in a GameState class
    Friend Function GetState() As clsGameState
        Dim NewState As New clsGameState

        With NewState

            .sOutputText = UserSession.sTurnOutput

            For Each loc As clsLocation In Adventure.htblLocations.Values
                Dim locs As New clsGameState.clsLocationState
                For Each sPropKey As String In loc.htblActualProperties.Keys
                    Dim prop As clsProperty = loc.htblProperties(sPropKey)
                    Dim props As New clsGameState.clsLocationState.clsStateProperty
                    props.Value = prop.Value(True)
                    locs.htblProperties.Add(sPropKey, props)
                Next
                SaveDisplayOnce(loc.AllDescriptions, locs.htblDisplayedDescriptions)
                locs.bSeen = Adventure.Player.HasSeenLocation(loc.Key)
                .htblLocationStates.Add(loc.Key, locs)
            Next

            For Each ob As clsObject In Adventure.htblObjects.Values
                Dim obs As New clsGameState.clsObjectState
                obs.Location = ob.Location.Copy
                For Each sPropKey As String In ob.htblActualProperties.Keys
                    Dim prop As clsProperty = ob.htblProperties(sPropKey)
                    Dim props As New clsGameState.clsObjectState.clsStateProperty
                    props.Value = prop.Value(True)
                    obs.htblProperties.Add(sPropKey, props)
                Next
                SaveDisplayOnce(ob.AllDescriptions, obs.htblDisplayedDescriptions)
                .htblObjectStates.Add(ob.Key, obs)
            Next

            For Each tas As clsTask In Adventure.htblTasks.Values
                Dim tass As New clsGameState.clsTaskState
                tass.Completed = tas.Completed
                tass.Scored = tas.Scored
                SaveDisplayOnce(tas.AllDescriptions, tass.htblDisplayedDescriptions)
                .htblTaskStates.Add(tas.Key, tass)
            Next

            For Each ev As clsEvent In Adventure.htblEvents.Values
                Dim evs As New clsGameState.clsEventState
                evs.Status = ev.Status
                evs.TimerToEndOfEvent = ev.TimerToEndOfEvent
                evs.iLastSubEventTime = ev.iLastSubEventTime
                For i As Integer = 0 To ev.SubEvents.Length - 1
                    If ev.LastSubEvent Is ev.SubEvents(i) Then
                        evs.iLastSubEventIndex = i
                        Exit For
                    End If
                Next
                SaveDisplayOnce(ev.AllDescriptions, evs.htblDisplayedDescriptions)
                .htblEventStates.Add(ev.Key, evs)
            Next

            For Each ch As clsCharacter In Adventure.htblCharacters.Values
                Dim chs As New clsGameState.clsCharacterState
                chs.Location = ch.Location
                For Each w As clsWalk In ch.arlWalks
                    Dim ws As New clsGameState.clsCharacterState.clsWalkState
                    ws.Status = w.Status
                    ws.TimerToEndOfWalk = w.iTimerToEndOfWalk
                    chs.lWalks.Add(ws)
                Next
                chs.lSeenKeys.Clear()
                For Each sLocKey As String In Adventure.htblLocations.Keys
                    If ch.HasSeenLocation(sLocKey) Then chs.lSeenKeys.Add(sLocKey)
                Next
                For Each sObKey As String In Adventure.htblObjects.Keys
                    If ch.HasSeenObject(sObKey) Then chs.lSeenKeys.Add(sObKey)
                Next
                For Each sChKey As String In Adventure.htblCharacters.Keys
                    If ch.HasSeenCharacter(sChKey) Then chs.lSeenKeys.Add(sChKey)
                Next
                For Each sPropKey As String In ch.htblActualProperties.Keys
                    Dim prop As clsProperty = ch.htblProperties(sPropKey)
                    Dim props As New clsGameState.clsCharacterState.clsStateProperty
                    props.Value = prop.Value(True)
                    chs.htblProperties.Add(sPropKey, props)
                Next
                If Not chs.htblProperties.ContainsKey("ProperName") Then
                    Dim props As New clsGameState.clsCharacterState.clsStateProperty
                    props.Value = ch.ProperName
                    chs.htblProperties.Add("ProperName", props)
                End If
                SaveDisplayOnce(ch.AllDescriptions, chs.htblDisplayedDescriptions)
                .htblCharacterStates.Add(ch.Key, chs)
            Next

            For Each var As clsVariable In Adventure.htblVariables.Values
                Dim vars As New clsGameState.clsVariableState
                ReDim vars.Value(var.Length - 1)
                For i As Integer = 0 To var.Length - 1
                    If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                        vars.Value(i) = var.IntValue(i + 1).ToString
                    Else
                        vars.Value(i) = var.StringValue(i + 1)
                    End If
                Next
                SaveDisplayOnce(var.AllDescriptions, vars.htblDisplayedDescriptions)
                ' TODO: Arrays
                .htblVariableStates.Add(var.Key, vars)
            Next
            For Each grp As clsGroup In Adventure.htblGroups.Values
                Dim grps As New clsGameState.clsGroupState
                For Each sMembers As String In grp.arlMembers
                    grps.lstMembers.Add(sMembers)
                Next
                .htblGroupStates.Add(grp.Key, grps)
            Next

        End With

        Return NewState

    End Function

    ' Save the current game state onto our stack
    Public Sub RecordState()
        Push(GetState)
    End Sub

    ' Load from file, and restore
    Friend Sub LoadState(ByVal sFilePath As String)
        Dim NewState As clsGameState = Nothing
        LoadFile(sFilePath, FileTypeEnum.GameState_TAS, LoadWhatEnum.All, False, , NewState)
        If NewState IsNot Nothing Then RestoreState(NewState)
    End Sub

    Private Sub RestoreDisplayOnce(ByVal AllDescriptions As List(Of Description), ByVal htblStore As Dictionary(Of String, Boolean))

        Dim iDesc As Integer = 0
        For Each d As Description In AllDescriptions
            iDesc += 1
            Dim iSing As Integer = 0
            For Each sd As SingleDescription In d
                iSing += 1
                If sd.DisplayOnce Then
                    Dim sKey As String = iDesc & "-" & iSing
                    If htblStore.ContainsKey(sKey) Then sd.Displayed = True Else sd.Displayed = False
                End If
            Next
        Next

    End Sub

    Friend Sub RestoreState(ByVal state As clsGameState)

        If state Is Nothing Then Exit Sub

        With state

            UserSession.sTurnOutput = .sOutputText

            For Each loc As clsLocation In Adventure.htblLocations.Values
                If .htblLocationStates.ContainsKey(loc.Key) Then
                    Dim locs As clsGameState.clsLocationState = CType(.htblLocationStates(loc.Key), clsGameState.clsLocationState)
                    Dim lDelete As New List(Of String)
                    For Each prop As clsProperty In loc.htblProperties.Values
                        If locs.htblProperties.ContainsKey(prop.Key) Then
                            Dim props As clsGameState.clsLocationState.clsStateProperty = CType(locs.htblProperties(prop.Key), clsGameState.clsLocationState.clsStateProperty)
                            prop.Value = props.Value
                        Else
                            lDelete.Add(prop.Key)
                        End If
                    Next
                    For Each sKey As String In lDelete
                        loc.RemoveProperty(sKey)
                    Next
                    For Each sPropKey As String In locs.htblProperties.Keys
                        If Not loc.htblActualProperties.ContainsKey(sPropKey) Then
                            If Adventure.htblLocationProperties.ContainsKey(sPropKey) Then
                                Dim propAdd As clsProperty = CType(Adventure.htblLocationProperties(sPropKey).Clone, clsProperty)
                                If propAdd.Type = clsProperty.PropertyTypeEnum.SelectionOnly Then
                                    propAdd.Selected = True
                                    loc.AddProperty(propAdd)
                                End If
                            End If
                        End If
                    Next
                    loc.ResetInherited()
                    RestoreDisplayOnce(loc.AllDescriptions, locs.htblDisplayedDescriptions)
                End If
            Next

            For Each ob As clsObject In Adventure.htblObjects.Values
                If .htblObjectStates.ContainsKey(ob.Key) Then
                    Dim obs As clsGameState.clsObjectState = CType(.htblObjectStates(ob.Key), clsGameState.clsObjectState)
                    ob.Location = obs.Location.Copy
                    Dim lDelete As New List(Of String)
                    For Each prop As clsProperty In ob.htblProperties.Values
                        If obs.htblProperties.ContainsKey(prop.Key) Then
                            Dim props As clsGameState.clsObjectState.clsStateProperty = CType(obs.htblProperties(prop.Key), clsGameState.clsObjectState.clsStateProperty)
                            prop.Value = props.Value
                        Else
                            lDelete.Add(prop.Key)
                        End If
                    Next
                    For Each sKey As String In lDelete
                        ob.RemoveProperty(sKey)
                    Next
                    For Each sPropKey As String In obs.htblProperties.Keys
                        If Not ob.htblActualProperties.ContainsKey(sPropKey) Then
                            If Adventure.htblObjectProperties.ContainsKey(sPropKey) Then
                                Dim propAdd As clsProperty = CType(Adventure.htblObjectProperties(sPropKey).Clone, clsProperty)
                                If propAdd.Type = clsProperty.PropertyTypeEnum.SelectionOnly Then
                                    propAdd.Selected = True
                                    ob.AddProperty(propAdd)
                                End If
                            End If
                        End If
                    Next
                    ob.ResetInherited()
                    RestoreDisplayOnce(ob.AllDescriptions, obs.htblDisplayedDescriptions)
                End If
            Next

            For Each tas As clsTask In Adventure.htblTasks.Values
                If .htblTaskStates.ContainsKey(tas.Key) Then
                    Dim tass As clsGameState.clsTaskState = CType(.htblTaskStates(tas.Key), clsGameState.clsTaskState)
                    tas.Completed = tass.Completed
                    tas.Scored = tass.Scored
                    RestoreDisplayOnce(tas.AllDescriptions, tass.htblDisplayedDescriptions)
                End If
            Next

            For Each ev As clsEvent In Adventure.htblEvents.Values
                If .htblEventStates.ContainsKey(ev.Key) Then
                    Dim evs As clsGameState.clsEventState = CType(.htblEventStates(ev.Key), clsGameState.clsEventState)
                    ev.Status = evs.Status
                    ev.TimerToEndOfEvent = evs.TimerToEndOfEvent
                    ev.iLastSubEventTime = evs.iLastSubEventTime
                    If ev.SubEvents IsNot Nothing AndAlso ev.SubEvents.Length > evs.iLastSubEventIndex Then ev.LastSubEvent = ev.SubEvents(evs.iLastSubEventIndex)
                    RestoreDisplayOnce(ev.AllDescriptions, evs.htblDisplayedDescriptions)
                End If
            Next

            For Each ch As clsCharacter In Adventure.htblCharacters.Values
                If .htblCharacterStates.ContainsKey(ch.Key) Then
                    Dim chs As clsGameState.clsCharacterState = CType(.htblCharacterStates(ch.Key), clsGameState.clsCharacterState)
                    ch.Location = chs.Location
                    If ch.arlWalks.Count = chs.lWalks.Count Then
                        For i As Integer = 0 To ch.arlWalks.Count - 1
                            Dim w As clsWalk = ch.arlWalks(i)
                            Dim ws As clsGameState.clsCharacterState.clsWalkState = chs.lWalks(i)
                            w.Status = ws.Status
                            w.iTimerToEndOfWalk = ws.TimerToEndOfWalk
                        Next
                    End If
                    For Each sLocKey As String In Adventure.htblLocations.Keys
                        ch.HasSeenLocation(sLocKey) = chs.lSeenKeys.Contains(sLocKey)
                    Next
                    For Each sObKey As String In Adventure.htblObjects.Keys
                        ch.HasSeenObject(sObKey) = chs.lSeenKeys.Contains(sObKey)
                    Next
                    For Each sChKey As String In Adventure.htblCharacters.Keys
                        ch.HasSeenCharacter(sChKey) = chs.lSeenKeys.Contains(sChKey)
                    Next
                    Dim lDelete As New List(Of String)
                    For Each prop As clsProperty In ch.htblProperties.Values
                        If chs.htblProperties.ContainsKey(prop.Key) Then
                            Dim props As clsGameState.clsCharacterState.clsStateProperty = CType(chs.htblProperties(prop.Key), clsGameState.clsCharacterState.clsStateProperty)
                            prop.Value = props.Value
                        Else
                            lDelete.Add(prop.Key)
                        End If
                    Next
                    For Each sKey As String In lDelete
                        ch.RemoveProperty(sKey)
                    Next
                    For Each sPropKey As String In chs.htblProperties.Keys
                        If Not ch.htblActualProperties.ContainsKey(sPropKey) Then
                            If Adventure.htblCharacterProperties.ContainsKey(sPropKey) Then
                                Dim propAdd As clsProperty = CType(Adventure.htblCharacterProperties(sPropKey).Clone, clsProperty)
                                If propAdd.Type = clsProperty.PropertyTypeEnum.SelectionOnly Then
                                    propAdd.Selected = True
                                    ch.AddProperty(propAdd)
                                End If
                            Else
                                Select Case sPropKey
                                    Case "ProperName"
                                        ch.ProperName = chs.htblProperties(sPropKey).Value
                                End Select
                            End If
                        End If
                    Next
                    ch.ResetInherited()
                    RestoreDisplayOnce(ch.AllDescriptions, chs.htblDisplayedDescriptions)
                End If
            Next

            For Each var As clsVariable In Adventure.htblVariables.Values
                If .htblVariableStates.ContainsKey(var.Key) Then
                    Dim vars As clsGameState.clsVariableState = CType(.htblVariableStates(var.Key), clsGameState.clsVariableState)
                    For i As Integer = 0 To var.Length - 1
                        If var.Type = clsVariable.VariableTypeEnum.Numeric Then var.IntValue(i + 1) = SafeInt(vars.Value(i)) Else var.StringValue(i + 1) = vars.Value(i)
                    Next
                    RestoreDisplayOnce(var.AllDescriptions, vars.htblDisplayedDescriptions)
                End If
            Next

            For Each grp As clsGroup In Adventure.htblGroups.Values
                If .htblGroupStates.ContainsKey(grp.Key) Then
                    Dim grps As clsGameState.clsGroupState = CType(.htblGroupStates(grp.Key), clsGameState.clsGroupState)
                    Dim lMembersToReset As New List(Of String)
                    For Each sMember As String In grp.arlMembers
                        lMembersToReset.Add(sMember)
                    Next
                    grp.arlMembers.Clear()
                    For Each sMember As String In grps.lstMembers
                        grp.arlMembers.Add(sMember)
                        If lMembersToReset.Contains(sMember) Then lMembersToReset.Remove(sMember) Else lMembersToReset.Add(sMember)
                    Next
                    For Each sMember As String In lMembersToReset
                        If Adventure.dictAllItems.ContainsKey(sMember) Then CType(Adventure.dictAllItems(sMember), clsItemWithProperties).ResetInherited()
                    Next
                End If
            Next

        End With

    End Sub


    Private stateLast As clsGameState
    Private stateCurrent As clsGameState

    Public Function SetLastState() As Boolean
        If MyBase.Count > 1 Then
            MyBase.Pop() ' Discard current state
            RestoreState(CType(MyBase.Peek, clsGameState))
            Adventure.eGameState = clsAction.EndGameEnum.Running
            Debug.WriteLine("Popped (" & MyBase.Count & " on stack)")
            Return True
        Else
            Return False
        End If
    End Function

    Public Sub SetCurrentState()
        RestoreState(stateCurrent)
    End Sub

    Public Shadows Sub Clear()
        MyBase.Clear()
        stateLast = Nothing
        stateCurrent = Nothing
    End Sub

End Class


Friend Class clsGameState
    Friend sOutputText As String
    Friend Class clsObjectState
        Friend Class clsStateProperty
            Friend Value As String
        End Class

        Public Location As clsObjectLocation
        Friend htblProperties As New Dictionary(Of String, clsStateProperty)
        Friend htblDisplayedDescriptions As New Dictionary(Of String, Boolean)
    End Class
    Public htblObjectStates As New Dictionary(Of String, clsObjectState)

    Friend Class clsCharacterState
        Friend Class clsWalkState
            Friend Status As clsWalk.StatusEnum
            Friend TimerToEndOfWalk As Integer
        End Class
        <DebuggerDisplay("CharState={Value}")>
        Friend Class clsStateProperty
            Friend Value As String
        End Class

        Friend Location As clsCharacterLocation
        Friend lWalks As New List(Of clsWalkState)
        Friend lSeenKeys As New List(Of String)
        Friend htblProperties As New Dictionary(Of String, clsStateProperty)
        Friend htblDisplayedDescriptions As New Dictionary(Of String, Boolean)
    End Class
    Public htblCharacterStates As New Dictionary(Of String, clsCharacterState)

    Friend Class clsTaskState
        Friend Completed As Boolean
        Friend Scored As Boolean
        Friend htblDisplayedDescriptions As New Dictionary(Of String, Boolean)
    End Class
    Friend htblTaskStates As New Dictionary(Of String, clsTaskState)

    Friend Class clsEventState
        Friend Status As clsEvent.StatusEnum
        Friend TimerToEndOfEvent As Integer
        Friend iLastSubEventTime As Integer
        Friend iLastSubEventIndex As Integer
        Friend htblDisplayedDescriptions As New Dictionary(Of String, Boolean)
    End Class
    Friend htblEventStates As New Dictionary(Of String, clsEventState)

    Friend Class clsVariableState
        Friend Value() As String
        Friend htblDisplayedDescriptions As New Dictionary(Of String, Boolean)
    End Class
    Friend htblVariableStates As New Dictionary(Of String, clsVariableState)

    Friend Class clsLocationState
        Friend Class clsStateProperty
            Friend Value As String
        End Class

        Friend htblProperties As New Dictionary(Of String, clsStateProperty)
        Friend htblDisplayedDescriptions As New Dictionary(Of String, Boolean)
        Friend bSeen As Boolean
    End Class
    Public htblLocationStates As New Dictionary(Of String, clsLocationState)

    Friend Class clsGroupState
        Friend lstMembers As New List(Of String)
    End Class
    Friend htblGroupStates As New Dictionary(Of String, clsGroupState)

End Class
