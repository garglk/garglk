Imports System.IO
Imports System.IO.Compression
Imports System.Xml

Imports FrankenDrift.Glue

Imports DashStyles = FrankenDrift.Glue.ConceptualDashStyle

Module FileIO

    Private br As BinaryReader
    Private bw As BinaryWriter
    Private sLoadString As String
    Private bAdventure() As Byte
    Private salWithStates As New StringArrayList
    Private iStartPriority As Integer
    Private dFileVersion As Double
    Friend Enum LoadWhatEnum
        All
        Properies
        AllExceptProperties
    End Enum


    Private Function LoadDescription(ByVal nodContainerXML As XmlElement, ByVal sNode As String) As Description

        Dim Description As New Description

        If nodContainerXML.Item(sNode) Is Nothing OrElse nodContainerXML.GetElementsByTagName(sNode).Count = 0 Then Return Description

        Dim nodDescription As XmlElement = CType(nodContainerXML.GetElementsByTagName(sNode)(0), XmlElement)

        For Each nodDesc As XmlElement In nodDescription.SelectNodes("Description") '. nodDescription.GetElementsByTagName("Description")
            Dim sd As New SingleDescription
            sd.Restrictions = LoadRestrictions(nodDesc)
            sd.eDisplayWhen = CType([Enum].Parse(GetType(SingleDescription.DisplayWhenEnum), nodDesc.Item("DisplayWhen").InnerText), SingleDescription.DisplayWhenEnum)
            If nodDesc.Item("Text") IsNot Nothing Then sd.Description = nodDesc.Item("Text").InnerText
            If nodDesc.Item("DisplayOnce") IsNot Nothing Then sd.DisplayOnce = GetBool(nodDesc.Item("DisplayOnce").InnerText)
            If nodDesc.Item("ReturnToDefault") IsNot Nothing Then sd.ReturnToDefault = GetBool(nodDesc.Item("ReturnToDefault").InnerText)
            If nodDesc.Item("TabLabel") IsNot Nothing Then sd.sTabLabel = SafeString(nodDesc.Item("TabLabel").InnerText)
            Description.Add(sd)
        Next

        If Description.Count > 1 Then Description.RemoveAt(0)
        Return Description

    End Function


    ' Write a state to file
    Friend Function SaveState(ByVal state As clsGameState, ByVal sFilePath As String) As Boolean

        Try
            Dim stmMemory As New MemoryStream
            Dim xmlWriter As New System.Xml.XmlTextWriter(stmMemory, System.Text.Encoding.UTF8)
            Dim bData() As Byte

            With xmlWriter
                .Indentation = 4 ' Change later

                .WriteStartDocument()
                .WriteStartElement("Game")

                ' TODO: Ideally this should only save values that are different from the initial TAF state                

                For Each sKey As String In state.htblLocationStates.Keys
                    Dim locs As clsGameState.clsLocationState = CType(state.htblLocationStates(sKey), clsGameState.clsLocationState)
                    .WriteStartElement("Location")
                    .WriteElementString("Key", sKey)
                    For Each sPropKey As String In locs.htblProperties.Keys
                        Dim props As clsGameState.clsLocationState.clsStateProperty = CType(locs.htblProperties(sPropKey), clsGameState.clsLocationState.clsStateProperty)
                        .WriteStartElement("Property")
                        .WriteElementString("Key", sPropKey)
                        .WriteElementString("Value", props.Value)
                        .WriteEndElement()
                    Next
                    For Each sDisplayedKey As String In locs.htblDisplayedDescriptions.Keys
                        .WriteElementString("Displayed", sDisplayedKey)
                    Next
                    .WriteEndElement()
                Next

                For Each sKey As String In state.htblObjectStates.Keys
                    Dim obs As clsGameState.clsObjectState = CType(state.htblObjectStates(sKey), clsGameState.clsObjectState)
                    .WriteStartElement("Object")
                    .WriteElementString("Key", sKey)
                    If obs.Location.DynamicExistWhere <> clsObjectLocation.DynamicExistsWhereEnum.Hidden Then
                        .WriteElementString("DynamicExistWhere", obs.Location.DynamicExistWhere.ToString)
                    End If
                    If obs.Location.StaticExistWhere <> clsObjectLocation.StaticExistsWhereEnum.NoRooms Then
                        .WriteElementString("StaticExistWhere", obs.Location.StaticExistWhere.ToString)
                    End If
                    .WriteElementString("LocationKey", obs.Location.Key)
                    For Each sPropKey As String In obs.htblProperties.Keys
                        Dim props As clsGameState.clsObjectState.clsStateProperty = CType(obs.htblProperties(sPropKey), clsGameState.clsObjectState.clsStateProperty)
                        .WriteStartElement("Property")
                        .WriteElementString("Key", sPropKey)
                        .WriteElementString("Value", props.Value)
                        .WriteEndElement()
                    Next
                    For Each sDisplayedKey As String In obs.htblDisplayedDescriptions.Keys
                        .WriteElementString("Displayed", sDisplayedKey)
                    Next
                    .WriteEndElement()
                Next

                For Each sKey As String In state.htblTaskStates.Keys
                    Dim tas As clsGameState.clsTaskState = CType(state.htblTaskStates(sKey), clsGameState.clsTaskState)
                    .WriteStartElement("Task")
                    .WriteElementString("Key", sKey)
                    .WriteElementString("Completed", tas.Completed.ToString)
                    .WriteElementString("Scored", tas.Scored.ToString)
                    For Each sDisplayedKey As String In tas.htblDisplayedDescriptions.Keys
                        .WriteElementString("Displayed", sDisplayedKey)
                    Next
                    .WriteEndElement()
                Next

                For Each sKey As String In state.htblEventStates.Keys
                    Dim evs As clsGameState.clsEventState = CType(state.htblEventStates(sKey), clsGameState.clsEventState)
                    .WriteStartElement("Event")
                    .WriteElementString("Key", sKey)
                    .WriteElementString("Status", evs.Status.ToString)
                    .WriteElementString("Timer", evs.TimerToEndOfEvent.ToString)
                    .WriteElementString("SubEventTime", evs.iLastSubEventTime.ToString)
                    .WriteElementString("SubEventIndex", evs.iLastSubEventIndex.ToString)
                    For Each sDisplayedKey As String In evs.htblDisplayedDescriptions.Keys
                        .WriteElementString("Displayed", sDisplayedKey)
                    Next
                    .WriteEndElement()
                Next

                For Each sKey As String In state.htblCharacterStates.Keys
                    Dim chs As clsGameState.clsCharacterState = CType(state.htblCharacterStates(sKey), clsGameState.clsCharacterState)
                    .WriteStartElement("Character")
                    .WriteElementString("Key", sKey)
                    If chs.Location.ExistWhere <> clsCharacterLocation.ExistsWhereEnum.Hidden Then
                        .WriteElementString("ExistWhere", chs.Location.ExistWhere.ToString)
                    End If
                    If chs.Location.Position <> clsCharacterLocation.PositionEnum.Standing Then
                        .WriteElementString("Position", chs.Location.Position.ToString)
                    End If
                    If chs.Location.Key <> "" Then .WriteElementString("LocationKey", chs.Location.Key)
                    For Each ws As clsGameState.clsCharacterState.clsWalkState In chs.lWalks
                        .WriteStartElement("Walk")
                        .WriteElementString("Status", ws.Status.ToString)
                        .WriteElementString("Timer", ws.TimerToEndOfWalk.ToString)
                        .WriteEndElement()
                    Next
                    For Each sSeen As String In chs.lSeenKeys
                        .WriteElementString("Seen", sSeen)
                    Next
                    For Each sPropKey As String In chs.htblProperties.Keys
                        Dim props As clsGameState.clsCharacterState.clsStateProperty = CType(chs.htblProperties(sPropKey), clsGameState.clsCharacterState.clsStateProperty)
                        .WriteStartElement("Property")
                        .WriteElementString("Key", sPropKey)
                        .WriteElementString("Value", props.Value)
                        .WriteEndElement()
                    Next
                    For Each sDisplayedKey As String In chs.htblDisplayedDescriptions.Keys
                        .WriteElementString("Displayed", sDisplayedKey)
                    Next
                    .WriteEndElement()
                Next

                For Each sKey As String In state.htblVariableStates.Keys
                    Dim vars As clsGameState.clsVariableState = CType(state.htblVariableStates(sKey), clsGameState.clsVariableState)
                    .WriteStartElement("Variable")
                    .WriteElementString("Key", sKey)

                    Dim v As clsVariable = Adventure.htblVariables(sKey)
                    For i As Integer = 0 To vars.Value.Length - 1
                        If v.Type = clsVariable.VariableTypeEnum.Numeric Then
                            If vars.Value(i) <> "0" Then .WriteElementString("Value_" & i, vars.Value(i))
                        Else
                            If vars.Value(i) <> "" Then .WriteElementString("Value_" & i, vars.Value(i))
                        End If
                    Next
                    For Each sDisplayedKey As String In vars.htblDisplayedDescriptions.Keys
                        .WriteElementString("Displayed", sDisplayedKey)
                    Next
                    .WriteEndElement()
                Next

                For Each sKey As String In state.htblGroupStates.Keys
                    Dim grps As clsGameState.clsGroupState = CType(state.htblGroupStates(sKey), clsGameState.clsGroupState)
                    .WriteStartElement("Group")
                    .WriteElementString("Key", sKey)
                    For Each sMember As String In grps.lstMembers
                        .WriteElementString("Member", sMember)
                    Next
                    .WriteEndElement()
                Next

                .WriteElementString("Turns", Adventure.Turns.ToString)

                .WriteEndElement() ' Game
                .WriteEndDocument()
                .Flush()

                Dim stmFile As New IO.FileStream(sFilePath, FileMode.Create)
                Using zStream As New ZLibStream(stmFile, CompressionLevel.Optimal)
                    stmMemory.Position = 0
                    stmMemory.CopyTo(zStream)
                End Using
            End With

            Return True

        Catch ex As Exception
            ErrMsg("Error saving game state", ex)
            Return False
        End Try

    End Function


    Friend Function LoadActions(ByVal nodContainerXML As XmlElement) As ActionArrayList

        Dim Actions As New ActionArrayList

        If nodContainerXML.Item("Actions") Is Nothing Then Return Actions

        For Each nod As XmlNode In nodContainerXML.Item("Actions").ChildNodes
            If TypeOf nod Is XmlElement Then
                Dim nodAct As XmlElement = CType(nod, XmlElement)
                Dim act As New clsAction
                Dim sAct As String
                Dim sType As String = Nothing
                sType = nodAct.Name

                If sType Is Nothing Then Return Actions

                sAct = nodAct.InnerText
                Dim sElements() As String = Split(sAct, " ")
                Select Case sType
                    Case "EndGame"
                        act.eItem = clsAction.ItemEnum.EndGame
                        act.eEndgame = EnumParseEndGame(sElements(0))

                    Case "MoveObject", "AddObjectToGroup", "RemoveObjectFromGroup"
                        Select Case sType
                            Case "MoveObject"
                                act.eItem = clsAction.ItemEnum.MoveObject
                            Case "AddObjectToGroup"
                                act.eItem = clsAction.ItemEnum.AddObjectToGroup
                            Case "RemoveObjectFromGroup"
                                act.eItem = clsAction.ItemEnum.RemoveObjectFromGroup
                        End Select

                        If dFileVersion <= 5.000016 Then ' Upgrade previous file format
                            act.sKey1 = sElements(0)
                            act.eMoveObjectWhat = clsAction.MoveObjectWhatEnum.Object
                            Select Case act.sKey1
                                Case "AllHeldObjects"
                                    act.eMoveObjectWhat = clsAction.MoveObjectWhatEnum.EverythingHeldBy
                                    act.sKey1 = THEPLAYER
                                Case "AllWornObjects"
                                    act.eMoveObjectWhat = clsAction.MoveObjectWhatEnum.EverythingWornBy
                                    act.sKey1 = THEPLAYER
                                Case Else
                                    ' Leave as is
                            End Select
                            act.eMoveObjectTo = EnumParseMoveObject(sElements(1))
                            act.sKey2 = sElements(2)
                        Else
                            act.eMoveObjectWhat = CType([Enum].Parse(GetType(clsAction.MoveObjectWhatEnum), sElements(0)), clsAction.MoveObjectWhatEnum)
                            act.sKey1 = sElements(1)
                            If sElements.Length > 4 Then
                                For iEl As Integer = 2 To sElements.Length - 3
                                    act.sPropertyValue &= sElements(iEl)
                                    If iEl < sElements.Length - 3 Then act.sPropertyValue &= " "
                                Next
                            End If
                            Select Case act.eItem
                                Case clsAction.ItemEnum.AddObjectToGroup
                                    act.eMoveObjectTo = clsAction.MoveObjectToEnum.ToGroup
                                Case clsAction.ItemEnum.RemoveObjectFromGroup
                                    act.eMoveObjectTo = clsAction.MoveObjectToEnum.FromGroup
                                Case clsAction.ItemEnum.MoveObject
                                    act.eMoveObjectTo = CType([Enum].Parse(GetType(clsAction.MoveObjectToEnum), sElements(sElements.Length - 2)), clsAction.MoveObjectToEnum)
                            End Select
                            act.sKey2 = sElements(sElements.Length - 1)
                        End If

                    Case "MoveCharacter", "AddCharacterToGroup", "RemoveCharacterFromGroup"
                        Select Case sType
                            Case "MoveCharacter"
                                act.eItem = clsAction.ItemEnum.MoveCharacter
                            Case "AddCharacterToGroup"
                                act.eItem = clsAction.ItemEnum.AddCharacterToGroup
                            Case "RemoveCharacterFromGroup"
                                act.eItem = clsAction.ItemEnum.RemoveCharacterFromGroup
                        End Select

                        If dFileVersion <= 5.000016 Then ' Upgrade previous file format
                            act.eItem = clsAction.ItemEnum.MoveCharacter
                            act.sKey1 = sElements(0)
                            act.eMoveCharacterTo = EnumParseMoveCharacter(sElements(1))
                            act.sKey2 = sElements(2)
                            If act.eMoveCharacterTo = clsAction.MoveCharacterToEnum.InDirection AndAlso IsNumeric(act.sKey2) Then
                                act.sKey2 = WriteEnum(CType(SafeInt(act.sKey2), DirectionsEnum))
                            End If
                        Else
                            act.eMoveCharacterWho = CType([Enum].Parse(GetType(clsAction.MoveCharacterWhoEnum), sElements(0)), clsAction.MoveCharacterWhoEnum)
                            act.sKey1 = sElements(1)
                            If sElements.Length > 4 Then
                                For iEl As Integer = 2 To sElements.Length - 3
                                    act.sPropertyValue &= sElements(iEl)
                                    If iEl < sElements.Length - 3 Then act.sPropertyValue &= " "
                                Next
                            End If
                            Select Case act.eItem
                                Case clsAction.ItemEnum.AddCharacterToGroup
                                    act.eMoveCharacterTo = clsAction.MoveCharacterToEnum.ToGroup
                                Case clsAction.ItemEnum.RemoveCharacterFromGroup
                                    act.eMoveCharacterTo = clsAction.MoveCharacterToEnum.FromGroup
                                Case clsAction.ItemEnum.MoveCharacter
                                    act.eMoveCharacterTo = CType([Enum].Parse(GetType(clsAction.MoveCharacterToEnum), sElements(sElements.Length - 2)), clsAction.MoveCharacterToEnum)
                            End Select
                            act.sKey2 = sElements(sElements.Length - 1)
                        End If

                    Case "AddLocationToGroup", "RemoveLocationFromGroup"
                        Select Case sType
                            Case "AddLocationToGroup"
                                act.eItem = clsAction.ItemEnum.AddLocationToGroup
                            Case "RemoveLocationFromGroup"
                                act.eItem = clsAction.ItemEnum.RemoveLocationFromGroup
                        End Select

                        act.eMoveLocationWhat = CType([Enum].Parse(GetType(clsAction.MoveLocationWhatEnum), sElements(0)), clsAction.MoveLocationWhatEnum)
                        act.sKey1 = sElements(1)
                        If sElements.Length > 4 Then
                            For iEl As Integer = 2 To sElements.Length - 3
                                act.sPropertyValue &= sElements(iEl)
                                If iEl < sElements.Length - 3 Then act.sPropertyValue &= " "
                            Next
                        End If
                        Select Case act.eItem
                            Case clsAction.ItemEnum.AddLocationToGroup
                                act.eMoveLocationTo = clsAction.MoveLocationToEnum.ToGroup
                            Case clsAction.ItemEnum.RemoveLocationFromGroup
                                act.eMoveLocationTo = clsAction.MoveLocationToEnum.FromGroup
                        End Select
                        act.sKey2 = sElements(sElements.Length - 1)

                    Case "SetProperty"
                        act.eItem = clsAction.ItemEnum.SetProperties
                        act.sKey1 = sElements(0)
                        act.sKey2 = sElements(1)
                        'act.StringValue = sElements(2)
                        Dim sValue As String = ""
                        For i As Integer = 2 To sElements.Length - 1
                            sValue &= sElements(i)
                            If i < sElements.Length - 1 Then sValue &= " "
                        Next
                        act.StringValue = sValue
                        act.sPropertyValue = sValue
                    Case "Score"
                    Case "SetTasks"
                        act.eItem = clsAction.ItemEnum.SetTasks
                        Dim iStartIndex As Integer = 0
                        Dim iEndIndex As Integer = 1
                        If sElements(0) = "FOR" Then
                            ' sElements(1) = %Loop%
                            ' sElements(2) = '='
                            act.IntValue = CInt(sElements(3))
                            ' sElements(4) = TO
                            act.sPropertyValue = sElements(5)
                            ' sElements(6) = :
                            iStartIndex = 7
                            iEndIndex = 4
                        End If
                        act.eSetTasks = EnumParseSetTask(sElements(iStartIndex))
                        act.sKey1 = sElements(iStartIndex + 1)
                        For iElement As Integer = iStartIndex + 2 To sElements.Length - iEndIndex
                            act.StringValue &= sElements(iElement)
                        Next
                        If act.StringValue IsNot Nothing Then
                            ' Simplify Runner so it only has to deal with multiple, or specific refs
                            act.StringValue = FixInitialRefs(act.StringValue)
                            If act.StringValue.StartsWith("(") Then act.StringValue = sRight(act.StringValue, act.StringValue.Length - 1)
                            If act.StringValue.EndsWith(")") Then act.StringValue = sLeft(act.StringValue, act.StringValue.Length - 1)
                        End If

                    Case "SetVariable", "IncVariable", "DecVariable", "ExecuteTask"
                        Select Case sType
                            Case "SetVariable"
                                act.eItem = clsAction.ItemEnum.SetVariable
                            Case "IncVariable"
                                act.eItem = clsAction.ItemEnum.IncreaseVariable
                            Case "DecVariable"
                                act.eItem = clsAction.ItemEnum.DecreaseVariable
                        End Select

                        If sElements(0) = "FOR" Then
                            act.eVariables = clsAction.VariablesEnum.Loop
                            ' sElements(1) = %Loop%
                            ' sElements(2) = '='
                            act.IntValue = CInt(sElements(3))
                            ' sElements(4) = TO
                            act.sKey2 = sElements(5)
                            ' sElements(6) = :
                            ' sElements(7) = SET
                            act.sKey1 = sElements(8).Split("["c)(0)
                            ' sElements(9) = '='
                            For iElement As Integer = 10 To sElements.Length - 4
                                act.StringValue &= sElements(iElement)
                                If iElement < sElements.Length - 4 Then act.StringValue &= " "
                            Next
                        Else
                            act.eVariables = clsAction.VariablesEnum.Assignment
                            If sInstr(sElements(0), "[") > 0 Then
                                act.sKey1 = sElements(0).Split("["c)(0)
                                act.sKey2 = sElements(0).Split("["c)(1).Replace("]", "")
                            Else
                                act.sKey1 = sElements(0)
                            End If
                            ' sElements(1) = '='
                            'act.StringValue = sElements(2)
                            For iElement As Integer = 2 To sElements.Length - 1
                                act.StringValue &= sElements(iElement)
                                If iElement < sElements.Length - 1 Then act.StringValue &= " "
                            Next
                            If dFileVersion > 5.0000321 Then
                                If act.StringValue.StartsWith("""") AndAlso act.StringValue.EndsWith("""") Then
                                    act.StringValue = act.StringValue.Substring(1, act.StringValue.Length - 2)
                                End If
                            End If
                        End If

                    Case "Time"
                        act.eItem = clsAction.ItemEnum.Time
                        For i As Integer = 1 To sElements.Length - 2
                            If i > 1 Then act.StringValue &= " "
                            act.StringValue &= sElements(i)
                        Next
                        act.StringValue = act.StringValue.Substring(1, act.StringValue.Length - 2)

                    Case "Conversation"
                        act.eItem = clsAction.ItemEnum.Conversation
                        Select Case sElements(0).ToUpper
                            Case "GREET", "FAREWELL"
                                If sElements(0).ToUpper = "GREET" Then
                                    act.eConversation = clsAction.ConversationEnum.Greet
                                Else
                                    act.eConversation = clsAction.ConversationEnum.Farewell
                                End If
                                act.sKey1 = sElements(1)
                                If sElements.Length > 2 Then
                                    ' sElements(2) = "with"
                                    For iElement As Integer = 3 To sElements.Length - 1
                                        act.StringValue &= sElements(iElement)
                                        If iElement < sElements.Length - 1 Then act.StringValue &= " "
                                    Next
                                    If act.StringValue.StartsWith("'") Then act.StringValue = act.StringValue.Substring(1)
                                    If act.StringValue.EndsWith("'") Then act.StringValue = sLeft(act.StringValue, act.StringValue.Length - 1)
                                End If

                            Case "ASK", "TELL"
                                If sElements(0).ToUpper = "ASK" Then
                                    act.eConversation = clsAction.ConversationEnum.Ask
                                Else
                                    act.eConversation = clsAction.ConversationEnum.Tell
                                End If
                                act.sKey1 = sElements(1)
                                ' sElements(2) = "About"
                                For iElement As Integer = 3 To sElements.Length - 1
                                    act.StringValue &= sElements(iElement)
                                    If iElement < sElements.Length - 1 Then act.StringValue &= " "
                                Next
                                If act.StringValue.StartsWith("'") Then act.StringValue = act.StringValue.Substring(1)
                                If act.StringValue.EndsWith("'") Then act.StringValue = sLeft(act.StringValue, act.StringValue.Length - 1)

                            Case "SAY"
                                act.eConversation = clsAction.ConversationEnum.Command
                                ' sElements(0) = "Say"
                                For iElement As Integer = 1 To sElements.Length - 3
                                    act.StringValue &= sElements(iElement)
                                    If iElement < sElements.Length - 3 Then act.StringValue &= " "
                                Next
                                If act.StringValue.StartsWith("'") Then act.StringValue = act.StringValue.Substring(1)
                                If act.StringValue.EndsWith("'") Then act.StringValue = sLeft(act.StringValue, act.StringValue.Length - 1)
                                ' sElements(len - 2) = "to"
                                act.sKey1 = sElements(sElements.Length - 1)

                            Case "ENTERWITH", "LEAVEWITH"
                                If sElements(0).ToUpper = "ENTERWITH" Then
                                    act.eConversation = clsAction.ConversationEnum.EnterConversation
                                Else
                                    act.eConversation = clsAction.ConversationEnum.LeaveConversation
                                End If
                                act.sKey1 = sElements(1)
                        End Select
                End Select

                Actions.Add(act)

            End If
        Next

        Return Actions

    End Function

    Friend Function LoadRestrictions(ByVal nodContainerXML As XmlElement) As RestrictionArrayList

        Dim Restrictions As New RestrictionArrayList

        If nodContainerXML.SelectNodes("Restrictions").Count = 0 Then Return Restrictions

        Dim nodRestrictions As XmlElement = CType(nodContainerXML.SelectNodes("Restrictions")(0), XmlElement)

        For Each nodRest As XmlElement In nodRestrictions.SelectNodes("Restriction")
            Dim rest As New clsRestriction
            Dim sRest As String
            Dim sType As String = Nothing
            If Not nodRest.Item("Location") Is Nothing Then sType = "Location"
            If Not nodRest.Item("Object") Is Nothing Then sType = "Object"
            If Not nodRest.Item("Task") Is Nothing Then sType = "Task"
            If Not nodRest.Item("Character") Is Nothing Then sType = "Character"
            If Not nodRest.Item("Variable") Is Nothing Then sType = "Variable"
            If Not nodRest.Item("Property") Is Nothing Then sType = "Property"
            If nodRest.Item("Direction") IsNot Nothing Then sType = "Direction"
            If nodRest.Item("Expression") IsNot Nothing Then sType = "Expression"
            If nodRest.Item("Item") IsNot Nothing Then sType = "Item"

            If nodRest.Item(sType) Is Nothing Then Exit For

            sRest = nodRest.Item(sType).InnerText
            Dim sElements() As String = Split(sRest, " ")
            Select Case sType
                Case "Location"
                    rest.eType = clsRestriction.RestrictionTypeEnum.Location
                    rest.sKey1 = sElements(0)
                    rest.eMust = EnumParseMust(sElements(1))

                    rest.eLocation = EnumParseLocation(sElements(2))
                    rest.sKey2 = sElements(3)
                Case "Object"
                    rest.eType = clsRestriction.RestrictionTypeEnum.Object
                    rest.sKey1 = sElements(0)
                    rest.eMust = EnumParseMust(sElements(1))
                    rest.eObject = EnumParseObject(sElements(2))
                    rest.sKey2 = sElements(3)
                Case "Task"
                    rest.eType = clsRestriction.RestrictionTypeEnum.Task
                    rest.sKey1 = sElements(0)
                    rest.eMust = EnumParseMust(sElements(1))
                    rest.eTask = clsRestriction.TaskEnum.Complete
                Case "Character"
                    rest.eType = clsRestriction.RestrictionTypeEnum.Character
                    rest.sKey1 = sElements(0)
                    rest.eMust = EnumParseMust(sElements(1))
                    rest.eCharacter = EnumParseCharacter(sElements(2))
                    rest.sKey2 = sElements(3)
                Case "Item"
                    rest.eType = clsRestriction.RestrictionTypeEnum.Item
                    rest.sKey1 = sElements(0)
                    rest.eMust = EnumParseMust(sElements(1))
                    rest.eItem = EnumParseItem(sElements(2))
                    rest.sKey2 = sElements(3)
                Case "Variable"
                    rest.eType = clsRestriction.RestrictionTypeEnum.Variable
                    rest.sKey1 = sElements(0)
                    If rest.sKey1.Contains("[") AndAlso rest.sKey1.Contains("]") Then
                        rest.sKey2 = rest.sKey1.Substring(rest.sKey1.IndexOf("[") + 1, rest.sKey1.LastIndexOf("]") - rest.sKey1.IndexOf("[") - 1)
                        rest.sKey1 = rest.sKey1.Substring(0, rest.sKey1.IndexOf("["))
                    End If
                    rest.eMust = EnumParseMust(sElements(1))
                    rest.eVariable = CType([Enum].Parse(GetType(clsRestriction.VariableEnum), sElements(2).Substring(2)), clsRestriction.VariableEnum)

                    Dim sValue As String = ""
                    For i As Integer = 3 To sElements.Length - 1
                        sValue &= sElements(i)
                        If i < sElements.Length - 1 Then sValue &= " "
                    Next
                    If sElements.Length = 4 AndAlso IsNumeric(sElements(3)) Then
                        rest.IntValue = CInt(sElements(3)) ' Integer value
                        rest.StringValue = rest.IntValue.ToString
                    Else
                        If sValue.StartsWith("""") AndAlso sValue.EndsWith("""") Then
                            rest.StringValue = sValue.Substring(1, sValue.Length - 2) ' String constant
                        ElseIf sValue.StartsWith("'") AndAlso sValue.EndsWith("'") Then
                            rest.StringValue = sValue.Substring(1, sValue.Length - 2) ' Expression
                        Else
                            rest.StringValue = sElements(3)
                            rest.IntValue = Integer.MinValue ' A key to a variable
                        End If
                    End If

                Case "Property"
                    rest.eType = clsRestriction.RestrictionTypeEnum.Property
                    rest.sKey1 = sElements(0)
                    rest.sKey2 = sElements(1)
                    rest.eMust = EnumParseMust(sElements(2))
                    Dim iStartExpression As Integer = 3
                    rest.IntValue = -1
                    For Each eEquals As clsRestriction.VariableEnum In [Enum].GetValues(GetType(clsRestriction.VariableEnum))
                        If sElements(3) = eEquals.ToString Then rest.IntValue = CInt(eEquals)
                    Next
                    If rest.IntValue > -1 Then iStartExpression = 4 Else rest.IntValue = CInt(clsRestriction.VariableEnum.EqualTo)
                    Dim sValue As String = ""
                    For i As Integer = iStartExpression To sElements.Length - 1
                        sValue &= sElements(i)
                        If i < sElements.Length - 1 Then sValue &= " "
                    Next
                    rest.StringValue = sValue

                Case "Direction"
                    rest.eType = clsRestriction.RestrictionTypeEnum.Direction
                    rest.eMust = EnumParseMust(sElements(0))
                    rest.sKey1 = sElements(1)
                    rest.sKey1 = sRight(rest.sKey1, rest.sKey1.Length - 2) ' Trim off the Be

                Case "Expression"
                    rest.eType = clsRestriction.RestrictionTypeEnum.Expression
                    rest.eMust = clsRestriction.MustEnum.Must
                    Dim sValue As String = ""
                    For i As Integer = 0 To sElements.Length - 1
                        sValue &= sElements(i)
                        If i < sElements.Length - 1 Then sValue &= " "
                    Next
                    rest.StringValue = sValue

            End Select
            rest.oMessage = LoadDescription(nodRest, "Message")
            Restrictions.Add(rest)

        Next
        Restrictions.BracketSequence = nodRestrictions.SelectNodes("BracketSequence")(0).InnerText ' GetElementsByTagName("BracketSequence")(0).InnerText

        If Not bAskedAboutBrackets AndAlso dFileVersion < 5.000026 AndAlso Restrictions.BracketSequence.Contains("#A#O#") Then
            bCorrectBracketSequences = Glue.AskYesNoQuestion("There was a logic correction in version 5.0.26 which means OR restrictions after AND restrictions were not evaluated.  Would you like to auto-correct these tasks?" & vbCrLf & vbCrLf & "You may not wish to do so if you have already used brackets around any OR restrictions following AND restrictions.", "Adventure Upgrade")
            bAskedAboutBrackets = True
        End If
        If bCorrectBracketSequences Then Restrictions.BracketSequence = CorrectBracketSequence(Restrictions.BracketSequence)

        Restrictions.BracketSequence = Restrictions.BracketSequence.Replace("[", "((").Replace("]", "))")
        Return Restrictions

    End Function

    Private Function FixInitialRefs(ByVal sCommand As String) As String
        If sCommand Is Nothing Then Return ""
        Return sCommand.Replace("%object%", "%object1%").Replace("%character%", "%character1%").Replace("%location%", "%location1%").Replace("%number%", "%number1%").Replace("%text%", "%text1%").Replace("%item%", "%item1%").Replace("%direction%", "%direction1%")
    End Function

    Private Function IsEqual(ByRef mb1 As Byte(), ByRef mb2 As Byte()) As Boolean

        If (mb1.Length <> mb2.Length) Then ' make sure arrays same length
            Return False
        Else
            For i As Integer = 0 To mb1.Length - 1 ' run array length looking for miscompare
                If mb1(i) <> mb2(i) Then Return False
            Next
        End If

        Return True
    End Function


    ' This loads from file into our data structure
    ' Assumes file exists
    '
    Public Enum FileTypeEnum
        TextAdventure_TAF
        XMLModule_AMF
        v4Module_AMF
        GameState_TAS
        Blorb
        Exe
    End Enum

    Friend Function LoadFile(ByVal sFilename As String, ByVal eFileType As FileTypeEnum, ByVal eLoadWhat As LoadWhatEnum, ByVal bLibrary As Boolean, Optional ByVal dtAdvDate As Date = #1/1/1900#, Optional ByRef state As clsGameState = Nothing, Optional ByVal lOffset As Long = 0, Optional ByVal bSilentError As Boolean = False) As Boolean

        Dim stmOriginalFile As IO.FileStream = Nothing

        Try
            If Not IO.File.Exists(sFilename) Then
                ErrMsg("File '" & sFilename & "' not found.")
                Return False
            End If

            stmOriginalFile = New IO.FileStream(sFilename, IO.FileMode.Open, FileAccess.Read)

            If eFileType = FileTypeEnum.TextAdventure_TAF Then

                stmOriginalFile.Seek(FileLen(sFilename) - 14, SeekOrigin.Begin)
                br = New BinaryReader(stmOriginalFile)
                Dim bPass As Byte() = Dencode(br.ReadBytes(12), FileLen(sFilename) - 13)
                Dim sPassString As String = System.Text.Encoding.UTF8.GetString(bPass)
                Adventure.Password = (sLeft(sPassString, 4) & sRight(sPassString, 4)).Trim

            End If

            If lOffset > 0 Then
                stmOriginalFile.Seek(lOffset, SeekOrigin.Begin)
                lOffset += 7 ' for the footer
            Else
                stmOriginalFile.Seek(0, SeekOrigin.Begin)
            End If

            br = New BinaryReader(stmOriginalFile)

            iLoading += 1

            Select Case eFileType
                Case FileTypeEnum.Exe
                    If clsBlorb.ExecResource IsNot Nothing AndAlso clsBlorb.ExecResource.Length > 0 Then
                        If Not Load500(Decompress(clsBlorb.ExecResource, dVersion >= 5.00002 AndAlso clsBlorb.bObfuscated, 16, clsBlorb.ExecResource.Length - 30), False) Then Return False
                        clsBlorb.bObfuscated = False
                        If clsBlorb.MetaData IsNot Nothing Then Adventure.BabelTreatyInfo.FromString(clsBlorb.MetaData.OuterXml)
                        Adventure.FullPath = SafeString(Application.ExecutablePath)
                    Else
                        Return False
                    End If

                Case FileTypeEnum.Blorb
                    Blorb = New clsBlorb
                    If Blorb.LoadBlorb(stmOriginalFile, sFilename) Then

                        Dim bVersion As Byte() = New Byte(11) {}
                        Array.Copy(clsBlorb.ExecResource, bVersion, 12)

                        Dim sVersion As String
                        If IsEqual(bVersion, New Byte() {60, 66, 63, 201, 106, 135, 194, 207, 146, 69, 62, 97}) Then
                            sVersion = "Version 5.00"
                        ElseIf IsEqual(bVersion, New Byte() {60, 66, 63, 201, 106, 135, 194, 207, 147, 69, 62, 97}) Then
                            sVersion = "Version 4.00"
                        ElseIf IsEqual(bVersion, New Byte() {60, 66, 63, 201, 106, 135, 194, 207, 148, 69, 55, 97}) Then
                            sVersion = "Version 3.90"
                        Else
                            sVersion = System.Text.Encoding.UTF8.GetString(Dencode(bVersion, 1))
                        End If

                        If Left(sVersion, 8) <> "Version " Then
                            ErrMsg("Not an ADRIFT Blorb file")
                            Return False
                        End If

                        UserSession.ShowUserSplash()

                        With Adventure
                            .dVersion = Double.Parse(sVersion.Replace("Version ", ""), Globalization.CultureInfo.InvariantCulture.NumberFormat) 'CDbl(sVersion.Replace("Version ", ""))
                            .Filename = Path.GetFileName(sFilename)
                            .FullPath = sFilename
                        End With

                        Dim bDeObfuscate As Boolean = clsBlorb.MetaData Is Nothing OrElse clsBlorb.MetaData.OuterXml.Contains("compilerversion") ' Nasty, but works
                        ' Was this a pre-obfuscated size blorb?
                        If clsBlorb.ExecResource.Length > 16 AndAlso clsBlorb.ExecResource(12) = 48 AndAlso clsBlorb.ExecResource(13) = 48 AndAlso clsBlorb.ExecResource(14) = 48 AndAlso clsBlorb.ExecResource(15) = 48 Then
                            If Not Load500(Decompress(clsBlorb.ExecResource, bDeObfuscate, 16, clsBlorb.ExecResource.Length - 30), False) Then Return False
                        Else
                            If Not Load500(Decompress(clsBlorb.ExecResource, bDeObfuscate, 12, clsBlorb.ExecResource.Length - 26), False) Then Return False
                        End If

                        If clsBlorb.MetaData IsNot Nothing Then Adventure.BabelTreatyInfo.FromString(clsBlorb.MetaData.OuterXml)
                    Else
                        Return False
                    End If

                Case FileTypeEnum.TextAdventure_TAF
                    Dim bVersion As Byte() = br.ReadBytes(12)
                    Dim sVersion As String
                    If IsEqual(bVersion, New Byte() {60, 66, 63, 201, 106, 135, 194, 207, 146, 69, 62, 97}) Then
                        sVersion = "Version 5.00"
                    ElseIf IsEqual(bVersion, New Byte() {60, 66, 63, 201, 106, 135, 194, 207, 147, 69, 62, 97}) Then
                        sVersion = "Version 4.00"
                    ElseIf IsEqual(bVersion, New Byte() {60, 66, 63, 201, 106, 135, 194, 207, 148, 69, 55, 97}) Then
                        sVersion = "Version 3.90"
                    Else
                        sVersion = System.Text.Encoding.UTF8.GetString(Dencode(bVersion, 1))
                    End If


                    If Left(sVersion, 8) <> "Version " Then
                        ErrMsg("Not an ADRIFT Text Adventure file")
                        Return False
                    End If

                    With Adventure
                        .dVersion = Double.Parse(sVersion.Replace("Version ", ""), Globalization.CultureInfo.InvariantCulture.NumberFormat)
                        .Filename = Path.GetFileName(sFilename)
                        .FullPath = sFilename
                    End With

                    Debug.WriteLine("Start Load: " & Now)
                    Select Case sVersion
                        Case "Version 3.90", "Version 4.00"
                            ErrMsg("ADRIFT v3.9 and v4 adventures are not supported in FrankenDrift.")
                            Return False
                        Case "Version 5.00"
                            Dim sSize As String = System.Text.Encoding.UTF8.GetString(br.ReadBytes(4))
                            Dim sCheck As String = System.Text.Encoding.UTF8.GetString(br.ReadBytes(8))
                            Dim iBabelLength As Integer = 0
                            Dim sBabel As String = Nothing
                            Dim bObfuscate As Boolean = True
                            If sSize = "0000" OrElse sCheck = "<ifindex" Then
                                stmOriginalFile.Seek(16, 0) ' Set to just after the size chunk
                                ' 5.0.20 format onwards
                                iBabelLength = CInt("&H" & sSize)
                                If iBabelLength > 0 Then
                                    Dim bBabel() As Byte = br.ReadBytes(iBabelLength)
                                    sBabel = System.Text.Encoding.UTF8.GetString(bBabel)
                                End If
                                iBabelLength += 4 ' For size header
                            Else
                                ' Pre 5.0.20 
                                ' THIS COULD BE AN EXTRACTED TAF, THEREFORE NO METADATA!!!
                                ' Ok, we have no uncompressed Babel info at the start.  Start over...
                                stmOriginalFile.Seek(0, SeekOrigin.Begin)
                                br = New BinaryReader(stmOriginalFile)
                                br.ReadBytes(12)
                                bObfuscate = False
                            End If
                            Dim stmFile As MemoryStream = FileToMemoryStream(True, CInt(FileLen(sFilename) - 26 - lOffset - iBabelLength), bObfuscate)
                            If stmFile Is Nothing Then Return False
                            If Not Load500(stmFile, False, False, eLoadWhat, dtAdvDate) Then Return False ' - 12)))
                            If sBabel <> "" Then
                                Adventure.BabelTreatyInfo.FromString(sBabel)
                                Dim sTemp As String = Adventure.CoverFilename
                                Adventure.CoverFilename = Nothing
                                Adventure.CoverFilename = sTemp ' Just to re-set the image in the Babel structure
                            End If

                        Case Else
                            ErrMsg("ADRIFT " & sVersion & " Adventures are not currently supported in ADRIFT v" & dVersion.ToString("0.0"))
                            Return False
                    End Select
                    Debug.WriteLine("End Load: " & Now)

                Case FileTypeEnum.v4Module_AMF
                    TODO("Version 4.0 Modules")

                Case FileTypeEnum.XMLModule_AMF
                    If Not Load500(FileToMemoryStream(False, CInt(FileLen(sFilename)), False), bLibrary, True, eLoadWhat, dtAdvDate, sFilename) Then Return False

                Case FileTypeEnum.GameState_TAS
                    state = LoadState(FileToMemoryStream(True, CInt(FileLen(sFilename)), False))

            End Select

            If br IsNot Nothing Then br.Close()
            br = Nothing
            stmOriginalFile.Close()
            stmOriginalFile = Nothing

            If Adventure.NotUnderstood = "" Then Adventure.NotUnderstood = "Sorry, I didn't understand that command."
            If Adventure.Player IsNot Nothing AndAlso (Adventure.Player.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.Hidden OrElse Adventure.Player.Location.Key = "") Then
                If Adventure.htblLocations.Count > 0 Then
                    Dim locFirst As clsCharacterLocation = Adventure.Player.Location
                    locFirst.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
                    For Each sKey As String In Adventure.htblLocations.Keys
                        locFirst.Key = sKey
                        Exit For
                    Next
                    Adventure.Player.Move(locFirst)
                End If
            End If

            iLoading -= 1

            Return True
        Catch ex As Exception
            If Not br Is Nothing Then br.Close()
            If Not stmOriginalFile Is Nothing Then stmOriginalFile.Close()
            ErrMsg("Error loading " & sFilename, ex)
            Return False
        Finally
        End Try

    End Function

    Private Function LoadState(ByVal stmMemory As MemoryStream) As clsGameState
        Try
            Dim NewState As New clsGameState
            Dim xmlDoc As New XmlDocument
            xmlDoc.Load(stmMemory)

            With xmlDoc.Item("Game")
                For Each nodLoc As XmlElement In xmlDoc.SelectNodes("/Game/Location")
                    With nodLoc
                        Dim locs As New clsGameState.clsLocationState
                        Dim sKey As String = .Item("Key").InnerText
                        For Each nodProp As XmlElement In nodLoc.SelectNodes("Property")
                            Dim props As New clsGameState.clsLocationState.clsStateProperty
                            Dim sPropKey As String = nodProp.Item("Key").InnerText
                            props.Value = nodProp.Item("Value").InnerText
                            locs.htblProperties.Add(sPropKey, props)
                        Next
                        For Each nodDisplayed As XmlElement In nodLoc.SelectNodes("Displayed")
                            locs.htblDisplayedDescriptions.Add(nodDisplayed.InnerText, True)
                        Next
                        NewState.htblLocationStates.Add(sKey, locs)
                    End With
                Next

                For Each nodOb As XmlElement In xmlDoc.SelectNodes("/Game/Object")
                    With nodOb
                        Dim obs As New clsGameState.clsObjectState
                        obs.Location = New clsObjectLocation
                        Dim sKey As String = .Item("Key").InnerText
                        If .Item("DynamicExistWhere") IsNot Nothing Then
                            obs.Location.DynamicExistWhere = CType([Enum].Parse(GetType(clsObjectLocation.DynamicExistsWhereEnum), .Item("DynamicExistWhere").InnerText), clsObjectLocation.DynamicExistsWhereEnum)
                        Else
                            obs.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.Hidden
                        End If
                        If .Item("StaticExistWhere") IsNot Nothing Then
                            obs.Location.StaticExistWhere = CType([Enum].Parse(GetType(clsObjectLocation.StaticExistsWhereEnum), .Item("StaticExistWhere").InnerText), clsObjectLocation.StaticExistsWhereEnum)
                        Else
                            obs.Location.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.NoRooms
                        End If
                        If .Item("LocationKey") IsNot Nothing Then obs.Location.Key = .Item("LocationKey").InnerText
                        For Each nodProp As XmlElement In nodOb.SelectNodes("Property")
                            Dim props As New clsGameState.clsObjectState.clsStateProperty
                            Dim sPropKey As String = nodProp.Item("Key").InnerText
                            props.Value = nodProp.Item("Value").InnerText
                            obs.htblProperties.Add(sPropKey, props)
                        Next
                        For Each nodDisplayed As XmlElement In nodOb.SelectNodes("Displayed")
                            obs.htblDisplayedDescriptions.Add(nodDisplayed.InnerText, True)
                        Next

                        NewState.htblObjectStates.Add(sKey, obs)
                    End With
                Next

                For Each nodTas As XmlElement In xmlDoc.SelectNodes("/Game/Task")
                    With nodTas
                        Dim tas As New clsGameState.clsTaskState
                        Dim sKey As String = .Item("Key").InnerText
                        tas.Completed = CBool(.Item("Completed").InnerText)
                        If .Item("Scored") IsNot Nothing Then tas.Scored = CBool(.Item("Scored").InnerText)
                        For Each nodDisplayed As XmlElement In nodTas.SelectNodes("Displayed")
                            tas.htblDisplayedDescriptions.Add(nodDisplayed.InnerText, True)
                        Next

                        NewState.htblTaskStates.Add(sKey, tas)
                    End With
                Next

                For Each nodEv As XmlElement In xmlDoc.SelectNodes("/Game/Event")
                    With nodEv
                        Dim evs As New clsGameState.clsEventState
                        Dim sKey As String = .Item("Key").InnerText
                        evs.Status = CType([Enum].Parse(GetType(clsEvent.StatusEnum), .Item("Status").InnerText), clsEvent.StatusEnum)
                        evs.TimerToEndOfEvent = SafeInt(.Item("Timer").InnerText)
                        If .Item("SubEventTime") IsNot Nothing Then evs.iLastSubEventTime = SafeInt(.Item("SubEventTime").InnerText)
                        If .Item("SubEventIndex") IsNot Nothing Then evs.iLastSubEventIndex = SafeInt(.Item("SubEventIndex").InnerText)
                        For Each nodDisplayed As XmlElement In nodEv.SelectNodes("Displayed")
                            evs.htblDisplayedDescriptions.Add(nodDisplayed.InnerText, True)
                        Next
                        NewState.htblEventStates.Add(sKey, evs)
                    End With
                Next

                For Each nodCh As XmlElement In xmlDoc.SelectNodes("/Game/Character")
                    With nodCh
                        Dim chs As New clsGameState.clsCharacterState
                        Dim sKey As String = .Item("Key").InnerText
                        If Adventure.htblCharacters.ContainsKey(sKey) Then
                            chs.Location = New clsCharacterLocation(Adventure.htblCharacters(sKey))
                            If .Item("ExistWhere") IsNot Nothing Then
                                chs.Location.ExistWhere = CType([Enum].Parse(GetType(clsCharacterLocation.ExistsWhereEnum), .Item("ExistWhere").InnerText), clsCharacterLocation.ExistsWhereEnum)
                            Else
                                chs.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.Hidden
                            End If
                            If .Item("Position") IsNot Nothing Then
                                chs.Location.Position = CType([Enum].Parse(GetType(clsCharacterLocation.PositionEnum), .Item("Position").InnerText), clsCharacterLocation.PositionEnum)
                            Else
                                chs.Location.Position = clsCharacterLocation.PositionEnum.Standing
                            End If
                            If .Item("LocationKey") IsNot Nothing Then
                                chs.Location.Key = .Item("LocationKey").InnerText
                            Else
                                chs.Location.Key = ""
                            End If
                            For Each nodW As XmlElement In nodCh.SelectNodes("Walk")
                                With nodW
                                    Dim ws As New clsGameState.clsCharacterState.clsWalkState
                                    ws.Status = CType([Enum].Parse(GetType(clsWalk.StatusEnum), .Item("Status").InnerText), clsWalk.StatusEnum)
                                    ws.TimerToEndOfWalk = SafeInt(.Item("Timer").InnerText)
                                    chs.lWalks.Add(ws)
                                End With
                            Next
                            For Each nodProp As XmlElement In nodCh.SelectNodes("Property")
                                Dim props As New clsGameState.clsCharacterState.clsStateProperty
                                Dim sPropKey As String = nodProp.Item("Key").InnerText
                                props.Value = nodProp.Item("Value").InnerText
                                chs.htblProperties.Add(sPropKey, props)
                            Next
                            For Each nodSeen As XmlElement In nodCh.SelectNodes("Seen")
                                chs.lSeenKeys.Add(nodSeen.InnerText)
                            Next
                            For Each nodDisplayed As XmlElement In nodCh.SelectNodes("Displayed")
                                chs.htblDisplayedDescriptions.Add(nodDisplayed.InnerText, True)
                            Next
                            NewState.htblCharacterStates.Add(sKey, chs)
                        Else
                            DisplayError("Character key " & sKey & " not found in adventure!")
                        End If
                    End With
                Next

                For Each nodVar As XmlElement In xmlDoc.SelectNodes("/Game/Variable")
                    With nodVar
                        Dim vars As New clsGameState.clsVariableState
                        Dim sKey As String = .Item("Key").InnerText
                        If Adventure.htblVariables.ContainsKey(sKey) Then
                            Dim v As clsVariable = Adventure.htblVariables(sKey)

                            ReDim vars.Value(v.Length - 1)
                            For i As Integer = 0 To v.Length - 1
                                If .Item("Value_" & i) IsNot Nothing Then
                                    vars.Value(i) = SafeString(.Item("Value_" & i).InnerText)
                                Else
                                    If v.Type = clsVariable.VariableTypeEnum.Numeric Then
                                        vars.Value(i) = "0"
                                    Else
                                        vars.Value(i) = ""
                                    End If
                                End If
                            Next
                            If .Item("Value") IsNot Nothing Then ' Old style                            
                                vars.Value(0) = SafeString(.Item("Value").InnerText)
                            End If

                            For Each nodDisplayed As XmlElement In nodVar.SelectNodes("Displayed")
                                vars.htblDisplayedDescriptions.Add(nodDisplayed.InnerText, True)
                            Next
                            NewState.htblVariableStates.Add(sKey, vars)
                        Else
                            DisplayError("Variable key " & sKey & " not found in adventure!")
                        End If

                    End With
                Next

                For Each nodGrp As XmlElement In xmlDoc.SelectNodes("/Game/Group")
                    With nodGrp
                        Dim grps As New clsGameState.clsGroupState
                        Dim sKey As String = .Item("Key").InnerText
                        For Each nodMember As XmlElement In nodGrp.SelectNodes("Member")
                            grps.lstMembers.Add(nodMember.InnerText)
                        Next
                        NewState.htblGroupStates.Add(sKey, grps)
                    End With
                Next

                If .Item("Turns") IsNot Nothing Then Adventure.Turns = SafeInt(.Item("Turns").InnerText)

            End With

            Return NewState

        Catch ex As Exception
            ErrMsg("Error loading game state", ex)
        End Try

        Return Nothing

    End Function

    Private Sub ObfuscateByteArray(ByRef bytData As Byte(), Optional ByVal iOffset As Integer = 0, Optional ByVal iLength As Integer = 0)

        Dim iRandomKey As Integer() = {41, 236, 221, 117, 23, 189, 44, 187, 161, 96, 4, 147, 90, 91, 172, 159, 244, 50, 249, 140, 190, 244, 82, 111, 170, 217, 13, 207, 25, 177, 18, 4, 3, 221, 160, 209, 253, 69, 131, 37, 132, 244, 21, 4, 39, 87, 56, 203, 119, 139, 231, 180, 190, 13, 213, 53, 153, 109, 202, 62, 175, 93, 161, 239, 77, 0, 143, 124, 186, 219, 161, 175, 175, 212, 7, 202, 223, 77, 72, 83, 160, 66, 88, 142, 202, 93, 70, 246, 8, 107, 55, 144, 122, 68, 117, 39, 83, 37, 183, 39, 199, 188, 16, 155, 233, 55, 5, 234, 6, 11, 86, 76, 36, 118, 158, 109, 5, 19, 36, 239, 185, 153, 115, 79, 164, 17, 52, 106, 94, 224, 118, 185, 150, 33, 139, 228, 49, 188, 164, 146, 88, 91, 240, 253, 21, 234, 107, 3, 166, 7, 33, 63, 0, 199, 109, 46, 72, 193, 246, 216, 3, 154, 139, 37, 148, 156, 182, 3, 235, 185, 60, 73, 111, 145, 151, 94, 169, 118, 57, 186, 165, 48, 195, 86, 190, 55, 184, 206, 180, 93, 155, 111, 197, 203, 143, 189, 208, 202, 105, 121, 51, 104, 24, 237, 203, 216, 208, 111, 48, 15, 132, 210, 136, 60, 51, 211, 215, 52, 102, 92, 227, 232, 79, 142, 29, 204, 131, 163, 2, 217, 141, 223, 12, 192, 134, 61, 23, 214, 139, 230, 102, 73, 158, 165, 216, 201, 231, 137, 152, 187, 230, 155, 99, 12, 149, 75, 25, 138, 207, 254, 85, 44, 108, 86, 129, 165, 197, 200, 182, 245, 187, 1, 169, 128, 245, 153, 74, 170, 181, 83, 229, 250, 11, 70, 243, 242, 123, 0, 42, 58, 35, 141, 6, 140, 145, 58, 221, 71, 35, 51, 4, 30, 210, 162, 0, 229, 241, 227, 22, 252, 1, 110, 212, 123, 24, 90, 32, 37, 99, 142, 42, 196, 158, 123, 209, 45, 250, 28, 238, 187, 188, 3, 134, 130, 79, 199, 39, 105, 70, 14, 0, 151, 234, 46, 56, 181, 185, 138, 115, 54, 25, 183, 227, 149, 9, 63, 128, 87, 208, 210, 234, 213, 244, 91, 63, 254, 232, 81, 44, 81, 51, 183, 222, 85, 142, 146, 218, 112, 66, 28, 116, 111, 168, 184, 161, 4, 31, 241, 121, 15, 70, 208, 152, 116, 35, 43, 163, 142, 238, 58, 204, 103, 94, 34, 2, 97, 217, 142, 6, 119, 100, 16, 20, 179, 94, 122, 44, 59, 185, 58, 223, 247, 216, 28, 11, 99, 31, 105, 49, 98, 238, 75, 129, 8, 80, 12, 17, 134, 181, 63, 43, 145, 234, 2, 170, 54, 188, 228, 22, 168, 255, 103, 213, 180, 91, 213, 143, 65, 23, 159, 66, 111, 92, 164, 136, 25, 143, 11, 99, 81, 105, 165, 133, 121, 14, 77, 12, 213, 114, 213, 166, 58, 83, 136, 99, 135, 118, 205, 173, 123, 124, 207, 111, 22, 253, 188, 52, 70, 122, 145, 167, 176, 129, 196, 63, 89, 225, 91, 165, 13, 200, 185, 207, 65, 248, 8, 27, 211, 64, 1, 162, 193, 94, 231, 213, 153, 53, 111, 124, 81, 25, 198, 91, 224, 45, 246, 184, 142, 73, 9, 165, 26, 39, 159, 178, 194, 0, 45, 29, 245, 161, 97, 5, 120, 238, 229, 81, 153, 239, 165, 35, 114, 223, 83, 244, 1, 94, 238, 20, 2, 79, 140, 137, 54, 91, 136, 153, 190, 53, 18, 153, 8, 81, 135, 176, 184, 193, 226, 242, 72, 164, 30, 159, 164, 230, 51, 58, 212, 171, 176, 100, 17, 25, 27, 165, 20, 215, 206, 29, 102, 75, 147, 100, 221, 11, 27, 32, 88, 162, 59, 64, 123, 252, 203, 93, 48, 237, 229, 80, 40, 77, 197, 18, 132, 173, 136, 238, 54, 225, 156, 225, 242, 197, 140, 252, 17, 185, 193, 153, 202, 19, 226, 49, 112, 111, 232, 20, 78, 190, 117, 38, 242, 125, 244, 24, 134, 128, 224, 47, 130, 45, 234, 119, 6, 90, 78, 182, 112, 206, 76, 118, 43, 75, 134, 20, 107, 147, 162, 20, 197, 116, 160, 119, 107, 117, 238, 116, 208, 115, 118, 144, 217, 146, 22, 156, 41, 107, 43, 21, 33, 50, 163, 127, 114, 254, 251, 166, 247, 223, 173, 242, 222, 203, 106, 14, 141, 114, 11, 145, 107, 217, 229, 253, 88, 187, 156, 153, 53, 233, 235, 255, 104, 141, 243, 146, 209, 33, 5, 109, 122, 72, 125, 240, 198, 131, 178, 14, 40, 8, 15, 182, 95, 153, 169, 71, 77, 166, 38, 182, 97, 97, 113, 13, 244, 173, 138, 80, 215, 215, 61, 107, 108, 157, 22, 35, 91, 244, 55, 213, 8, 142, 113, 44, 217, 52, 159, 206, 228, 171, 68, 42, 250, 78, 11, 24, 215, 112, 252, 24, 249, 97, 54, 80, 202, 164, 74, 194, 131, 133, 235, 88, 110, 81, 173, 211, 240, 68, 51, 191, 13, 187, 108, 44, 147, 18, 113, 30, 146, 253, 76, 235, 247, 30, 219, 167, 88, 32, 97, 53, 234, 221, 75, 94, 192, 236, 188, 169, 160, 56, 40, 146, 60, 61, 10, 62, 245, 10, 189, 184, 50, 43, 47, 133, 57, 0, 97, 80, 117, 6, 122, 207, 226, 253, 212, 48, 112, 14, 108, 166, 86, 199, 125, 89, 213, 185, 174, 186, 20, 157, 178, 78, 99, 169, 2, 191, 173, 197, 36, 191, 139, 107, 52, 154, 190, 88, 175, 63, 105, 218, 206, 230, 157, 22, 98, 107, 174, 214, 175, 127, 81, 166, 60, 215, 84, 44, 107, 57, 251, 21, 130, 170, 233, 172, 27, 234, 147, 227, 155, 125, 10, 111, 80, 57, 207, 203, 176, 77, 71, 151, 16, 215, 22, 165, 110, 228, 47, 92, 69, 145, 236, 118, 68, 84, 88, 35, 252, 241, 250, 119, 215, 203, 59, 50, 117, 225, 86, 2, 8, 137, 124, 30, 242, 99, 4, 171, 148, 68, 61, 55, 186, 55, 157, 9, 144, 147, 43, 252, 225, 171, 206, 190, 83, 207, 191, 68, 155, 227, 47, 140, 142, 45, 84, 188, 20}

        For i As Integer = 0 To bytData.Length - 1
            If i >= iOffset AndAlso (iLength = 0 OrElse i < iLength + iOffset) Then bytData(i) = CByte(bytData(i) Xor iRandomKey((i - iOffset) Mod 1024))
        Next

    End Sub


    Private Function Decompress(ByVal bZLib As Byte(), ByVal bObfuscate As Boolean, Optional ByVal iOffset As Integer = 0, Optional ByVal iLength As Integer = 0) As MemoryStream

        If bObfuscate Then ObfuscateByteArray(bZLib, iOffset, iLength)
        Dim outStream As New System.IO.MemoryStream
        If iLength = 0 Then iLength = bZLib.Length - iOffset
        Dim inStream As New System.IO.MemoryStream(bZLib, iOffset, iLength)
        Using zStream As New ZLibStream(inStream, CompressionMode.Decompress)
            zStream.CopyTo(outStream)
        End Using
        outStream.Position = 0
        Return outStream
    End Function

    Private Function FileToMemoryStream(ByVal bCompressed As Boolean, ByVal iLength As Integer, ByVal bObfuscate As Boolean) As MemoryStream

        'ReDim bAdventure(-1) ' if this needs to be done, why here?

        If bCompressed Then
            Dim bAdvZLib() As Byte = br.ReadBytes(iLength)
            Return Decompress(bAdvZLib, bObfuscate)
        Else
            Return New MemoryStream(br.ReadBytes(iLength))
        End If

    End Function


    ' Grab the numeric part of the key (if any) and increment it
    Private Function IncrementKey(ByVal sKey As String) As String

        Dim re As New System.Text.RegularExpressions.Regex("")

        Dim sJustKey As String = System.Text.RegularExpressions.Regex.Replace(sKey, "\d*$", "")
        Dim sNumber As String = sKey.Replace(sJustKey, "")
        If sNumber = "" Then sNumber = "0"
        Dim iNumber As Integer = CInt(sNumber)
        Return sJustKey & iNumber + 1

    End Function

    Private Class LoadAbortException
        Inherits Exception
    End Class

    Private Enum LoadItemEum As Integer
        No = 0
        Yes = 1
        Both = 2
    End Enum
    Private Function ShouldWeLoadLibraryItem(ByVal sItemKey As String) As LoadItemEum
        If Adventure.listExcludedItems.Contains(sItemKey) Then Return LoadItemEum.No
        Return LoadItemEum.Yes
    End Function


    Private Sub SetChar(ByRef sText As String, ByVal iPos As Integer, ByVal Character As Char)
        If iPos > sText.Length - 1 Then Exit Sub
        sText = sLeft(sText, iPos) & Character & sRight(sText, sText.Length - iPos - 1)
    End Sub


    Private Function GetDate(ByVal sDate As String) As Date

        ' Non UK locals save "yyyy-MM-dd HH:mm:ss" in their own format.  Convert these for pre-5.0.22 saves
        If dFileVersion < 5.000022 Then
            If sDate.Length = 19 Then
                SetChar(sDate, 13, ":"c)
                SetChar(sDate, 16, ":"c)
            End If
        End If

        Dim dtReturn As Date
        If Date.TryParse(sDate, dtReturn) Then Return dtReturn

        Return Date.MinValue

    End Function


    ' Corrects the bracket sequences for ORs after ANDs
    Private Function CorrectBracketSequence(ByVal sSequence As String) As String

        If sSequence.Contains("#A#O#") Then
            For i As Integer = 10 To 0 Step -1
                Dim sSearch As String = "#A#"
                For j As Integer = 0 To i
                    sSearch &= "O#"
                Next
                While sSequence.Contains(sSearch)
                    Dim sReplace As String = "#A(#"
                    For j As Integer = 0 To i
                        sReplace &= "O#"
                    Next
                    sReplace &= ")"
                    sSequence = sSequence.Replace(sSearch, sReplace)
                    iCorrectedTasks += 1
                End While
            Next
        End If

        Return sSequence

    End Function


    Private Function GetBool(ByVal sBool As String) As Boolean

        Select Case sBool.ToUpper
            Case "0", "FALSE"
                Return False
            Case "-1", "1", "TRUE", "VRAI"
                Return True
            Case Else
                Return False
        End Select

    End Function


    Dim bCorrectBracketSequences As Boolean = False
    Dim bAskedAboutBrackets As Boolean = False
    Dim iCorrectedTasks As Integer = 0

    Private Function Load500(ByVal stmMemory As MemoryStream, ByVal bLibrary As Boolean, Optional ByVal bAppend As Boolean = False, Optional ByVal eLoadWhat As LoadWhatEnum = LoadWhatEnum.All, Optional ByVal dtAdvDate As Date = #1/1/1900#, Optional ByVal sFilename As String = "") As Boolean
        Try
            If stmMemory Is Nothing Then Return False

            Dim xmlDoc As New XmlDocument
            xmlDoc.PreserveWhitespace = True
            xmlDoc.Load(stmMemory)
            Dim a As clsAdventure = Adventure
            Dim bAddDuplicateKeys As Boolean = True
            Dim htblDuplicateKeyMapping As New StringHashTable
            Dim arlNewTasks As New StringArrayList

            With xmlDoc.Item("Adventure")
                If .Item("Version") IsNot Nothing Then
                    dFileVersion = SafeDbl(.Item("Version").InnerText)
                    If dFileVersion > dVersion Then
                        Glue.MakeNote("This file is newer than what this software might understand.")
                    End If
                    If Not bLibrary Then a.dVersion = dFileVersion
                Else
                    Throw New Exception("Version tag not specified")
                End If

                If eLoadWhat = LoadWhatEnum.All Then
                    bAskedAboutBrackets = False
                    iCorrectedTasks = 0

                    If Not bLibrary AndAlso .Item("Title") IsNot Nothing Then a.Title = .Item("Title").InnerText
                    If Not bLibrary AndAlso .Item("Author") IsNot Nothing Then a.Author = .Item("Author").InnerText
                    If .Item("LastUpdated") IsNot Nothing Then a.LastUpdated = GetDate(.Item("LastUpdated").InnerText)
                    If Not .Item("Introduction") Is Nothing Then a.Introduction = LoadDescription(xmlDoc.Item("Adventure"), "Introduction") ' New Description(.Item("Introduction").InnerText)
                    If .Item("FontName") IsNot Nothing Then a.DefaultFontName = .Item("FontName").InnerText
                    If .Item("FontSize") IsNot Nothing Then a.DefaultFontSize = SafeInt(.Item("FontSize").InnerText)
                    If .Item("BackgroundColour") IsNot Nothing Then a.DeveloperDefaultBackgroundColour = CInt(.Item("BackgroundColour").InnerText)
                    If .Item("InputColour") IsNot Nothing Then a.DeveloperDefaultInputColour = CInt(.Item("InputColour").InnerText)
                    If .Item("OutputColour") IsNot Nothing Then a.DeveloperDefaultOutputColour = CInt(.Item("OutputColour").InnerText)
                    If .Item("LinkColour") IsNot Nothing Then a.DeveloperDefaultLinkColour = CInt(.Item("LinkColour").InnerText)
                    If .Item("ShowFirstLocation") IsNot Nothing Then a.ShowFirstRoom = GetBool(.Item("ShowFirstLocation").InnerText)
                    If .Item("UserStatus") IsNot Nothing Then a.sUserStatus = .Item("UserStatus").InnerText
                    If .Item("ifindex") IsNot Nothing Then Adventure.BabelTreatyInfo.FromString(.Item("ifindex").OuterXml) ' Pre 5.0.20
                    If .Item("Cover") IsNot Nothing Then
                        Adventure.CoverFilename = .Item("Cover").InnerText
                    End If
                    If .Item("ShowExits") IsNot Nothing Then a.ShowExits = GetBool(.Item("ShowExits").InnerText)
                    If .Item("EnableMenu") IsNot Nothing Then a.EnableMenu = GetBool(.Item("EnableMenu").InnerText)
                    If .Item("EnableDebugger") IsNot Nothing Then a.EnableDebugger = GetBool(.Item("EnableDebugger").InnerText)
                    If .Item("EndGameText") IsNot Nothing Then a.WinningText = LoadDescription(xmlDoc.Item("Adventure"), "EndGameText")
                    If .Item("Elapsed") IsNot Nothing Then a.iElapsed = SafeInt(.Item("Elapsed").InnerText)
                    If .Item("TaskExecution") IsNot Nothing Then a.TaskExecution = CType([Enum].Parse(GetType(clsAdventure.TaskExecutionEnum), .Item("TaskExecution").InnerText), clsAdventure.TaskExecutionEnum)
                    If .Item("WaitTurns") IsNot Nothing Then a.WaitTurns = SafeInt(.Item("WaitTurns").InnerText)
                    If .Item("KeyPrefix") IsNot Nothing Then
                        a.KeyPrefix = .Item("KeyPrefix").InnerText
                    End If

                    If .Item("DirectionNorth") IsNot Nothing AndAlso .Item("DirectionNorth").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.North) = .Item("DirectionNorth").InnerText
                    If .Item("DirectionNorthEast") IsNot Nothing AndAlso .Item("DirectionNorthEast").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.NorthEast) = .Item("DirectionNorthEast").InnerText
                    If .Item("DirectionEast") IsNot Nothing AndAlso .Item("DirectionEast").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.East) = .Item("DirectionEast").InnerText
                    If .Item("DirectionSouthEast") IsNot Nothing AndAlso .Item("DirectionSouthEast").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.SouthEast) = .Item("DirectionSouthEast").InnerText
                    If .Item("DirectionSouth") IsNot Nothing AndAlso .Item("DirectionSouth").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.South) = .Item("DirectionSouth").InnerText
                    If .Item("DirectionSouthWest") IsNot Nothing AndAlso .Item("DirectionSouthWest").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.SouthWest) = .Item("DirectionSouthWest").InnerText
                    If .Item("DirectionWest") IsNot Nothing AndAlso .Item("DirectionWest").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.West) = .Item("DirectionWest").InnerText
                    If .Item("DirectionNorthWest") IsNot Nothing AndAlso .Item("DirectionNorthWest").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.NorthWest) = .Item("DirectionNorthWest").InnerText
                    If .Item("DirectionIn") IsNot Nothing AndAlso .Item("DirectionIn").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.In) = .Item("DirectionIn").InnerText
                    If .Item("DirectionOut") IsNot Nothing AndAlso .Item("DirectionOut").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.Out) = .Item("DirectionOut").InnerText
                    If .Item("DirectionUp") IsNot Nothing AndAlso .Item("DirectionUp").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.Up) = .Item("DirectionUp").InnerText
                    If .Item("DirectionDown") IsNot Nothing AndAlso .Item("DirectionDown").InnerText <> "" Then a.sDirectionsRE(DirectionsEnum.Down) = .Item("DirectionDown").InnerText

                End If

                If eLoadWhat = LoadWhatEnum.All OrElse eLoadWhat = LoadWhatEnum.AllExceptProperties Then
                    Debug.WriteLine("End Intro: " & Now)
                End If


                If eLoadWhat = LoadWhatEnum.All OrElse eLoadWhat = LoadWhatEnum.Properies Then
                    ' Properties
                    For Each nodProp As XmlElement In xmlDoc.SelectNodes("/Adventure/Property")
                        Dim prop As New clsProperty
                        With nodProp
                            Dim sKey As String = .Item("Key").InnerText
                            If .Item("Library") IsNot Nothing Then prop.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblAllProperties.ContainsKey(sKey) Then
                                If a.htblAllProperties.ContainsKey(sKey) Then
                                    If prop.IsLibrary OrElse bLibrary Then
                                        If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblAllProperties(sKey).LastUpdated Then GoTo NextProp
                                        Select Case ShouldWeLoadLibraryItem(sKey)
                                            Case LoadItemEum.Yes
                                                a.htblAllProperties.Remove(sKey)
                                            Case LoadItemEum.No
                                                GoTo NextProp
                                            Case LoadItemEum.Both
                                                ' Keep key, but still add this new one
                                        End Select
                                    End If
                                End If
                                If bAddDuplicateKeys Then
                                    While a.htblAllProperties.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextProp
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextProp
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            prop.Key = sKey
                            If bLibrary Then
                                prop.IsLibrary = True
                                prop.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then prop.LastUpdated = GetDate(.Item("LastUpdated").InnerText)
                            prop.Description = .Item("Description").InnerText
                            If Not .Item("Mandatory") Is Nothing Then
                                prop.Mandatory = GetBool(.Item("Mandatory").InnerText)
                            End If
                            If .Item("PropertyOf") IsNot Nothing Then prop.PropertyOf = EnumParsePropertyPropertyOf(.Item("PropertyOf").InnerText)
                            If .Item("AppendTo") IsNot Nothing Then prop.AppendToProperty = .Item("AppendTo").InnerText
                            prop.Type = EnumParsePropertyType(.Item("Type").InnerText)
                            Select Case prop.Type
                                Case clsProperty.PropertyTypeEnum.StateList
                                    For Each nodState As XmlElement In nodProp.SelectNodes("State")
                                        prop.arlStates.Add(nodState.InnerText)
                                    Next
                                    If prop.arlStates.Count > 0 Then prop.Value = prop.arlStates(0)
                                Case clsProperty.PropertyTypeEnum.ValueList
                                    For Each nodValueList As XmlElement In nodProp.SelectNodes("ValueList")
                                        If nodValueList.Item("Label") IsNot Nothing Then
                                            Dim sLabel As String = nodValueList("Label").InnerText
                                            Dim iValue As Integer = 0
                                            If nodValueList.Item("Value") IsNot Nothing Then iValue = SafeInt(nodValueList("Value").InnerText)
                                            prop.ValueList.Add(sLabel, iValue)
                                        End If
                                    Next
                            End Select
                            If .Item("PrivateTo") IsNot Nothing Then prop.PrivateTo = .Item("PrivateTo").InnerText
                            If .Item("Tooltip") IsNot Nothing Then prop.PopupDescription = .Item("Tooltip").InnerText

                            If Not .Item("DependentKey") Is Nothing Then
                                If .Item("DependentKey").InnerText <> sKey Then
                                    prop.DependentKey = .Item("DependentKey").InnerText
                                    If Not .Item("DependentValue") Is Nothing Then
                                        prop.DependentValue = .Item("DependentValue").InnerText
                                    End If
                                End If
                            End If
                            If Not .Item("RestrictProperty") Is Nothing Then
                                prop.RestrictProperty = .Item("RestrictProperty").InnerText
                                If Not .Item("RestrictValue") Is Nothing Then
                                    prop.RestrictValue = .Item("RestrictValue").InnerText
                                End If
                            End If
                        End With
                        a.htblAllProperties.Add(prop)
NextProp:
                    Next
                    a.htblAllProperties.SetSelected()
                    Debug.WriteLine("End Properties: " & Now)

                    CreateMandatoryProperties()
                End If



                If eLoadWhat = LoadWhatEnum.All OrElse eLoadWhat = LoadWhatEnum.AllExceptProperties Then
                    ' Locations
                    For Each nodLoc As XmlElement In xmlDoc.SelectNodes("/Adventure/Location")
                        Dim loc As New clsLocation
                        With nodLoc
                            Dim sKey As String = .Item("Key").InnerText
                            If Not .Item("Library") Is Nothing Then loc.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblLocations.ContainsKey(sKey) Then
                                If a.htblLocations.ContainsKey(sKey) Then
                                    If loc.IsLibrary OrElse bLibrary Then
                                        If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblLocations(sKey).LastUpdated Then GoTo NextLoc
                                        Select Case ShouldWeLoadLibraryItem(sKey)
                                            Case LoadItemEum.Yes
                                                a.htblLocations.Remove(sKey)
                                            Case LoadItemEum.No
                                                GoTo NextLoc
                                            Case LoadItemEum.Both
                                                ' Keep key, but still add this new one
                                        End Select
                                    End If
                                End If
                                If bAddDuplicateKeys Then
                                    While a.htblLocations.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextLoc
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextLoc
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            loc.Key = sKey
                            If bLibrary Then
                                loc.IsLibrary = True
                                loc.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then loc.LastUpdated = CDate(.Item("LastUpdated").InnerText)
                            If dFileVersion < 5.000015 Then
                                loc.ShortDescription = New Description(.Item("ShortDescription").InnerText)
                            Else
                                loc.ShortDescription = LoadDescription(nodLoc, "ShortDescription")
                            End If
                            loc.LongDescription = LoadDescription(nodLoc, "LongDescription")

                            For Each nodDir As XmlElement In nodLoc.SelectNodes("Movement")
                                Dim xdir As DirectionsEnum = EnumParseDirections(nodDir.Item("Direction").InnerText)

                                With loc.arlDirections(xdir)
                                    .LocationKey = nodDir.Item("Destination").InnerText
                                    .Restrictions = LoadRestrictions(nodDir)
                                End With
                            Next
                            For Each nodProp As XmlElement In .SelectNodes("Property")
                                Dim prop As New clsProperty
                                Dim sPropKey As String = nodProp.Item("Key").InnerText
                                If Adventure.htblAllProperties.ContainsKey(sPropKey) Then
                                    prop = Adventure.htblAllProperties(sPropKey).Copy
                                    If prop.Type = clsProperty.PropertyTypeEnum.Text Then
                                        prop.StringData = LoadDescription(nodProp, "Value")
                                    ElseIf prop.Type <> clsProperty.PropertyTypeEnum.SelectionOnly Then
                                        prop.Value = nodProp.Item("Value").InnerText
                                    End If
                                    prop.Selected = True
                                    loc.AddProperty(prop)
                                End If
                            Next
                            If .Item("Hide") IsNot Nothing Then loc.HideOnMap = GetBool(.Item("Hide").InnerText)
                        End With
                        a.htblLocations.Add(loc, loc.Key)
NextLoc:
                    Next nodLoc
                    Debug.WriteLine("End Locations: " & Now)
                End If



                If eLoadWhat = LoadWhatEnum.All OrElse eLoadWhat = LoadWhatEnum.AllExceptProperties Then
                    ' Objects
                    For Each nodOb As XmlElement In .SelectNodes("/Adventure/Object")
                        Dim ob As New clsObject
                        With nodOb
                            Dim sKey As String = .Item("Key").InnerText
                            If Not .Item("Library") Is Nothing Then ob.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblObjects.ContainsKey(sKey) Then
                                If a.htblObjects.ContainsKey(sKey) Then
                                    If ob.IsLibrary OrElse bLibrary Then
                                        If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblObjects(sKey).LastUpdated Then GoTo NextOb
                                        Select Case ShouldWeLoadLibraryItem(sKey)
                                            Case LoadItemEum.Yes
                                                a.htblObjects.Remove(sKey)
                                            Case LoadItemEum.No
                                                GoTo NextOb
                                            Case LoadItemEum.Both
                                                ' Keep key, but still add this new one
                                        End Select
                                    End If
                                End If
                                If bAddDuplicateKeys Then
                                    While a.htblObjects.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextOb
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextOb
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            ob.Key = sKey
                            If bLibrary Then
                                ob.IsLibrary = True
                                ob.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then ob.LastUpdated = CDate(.Item("LastUpdated").InnerText)
                            If .Item("Article") IsNot Nothing Then ob.Article = .Item("Article").InnerText
                            If .Item("Prefix") IsNot Nothing Then ob.Prefix = .Item("Prefix").InnerText
                            For Each nodName As XmlElement In .GetElementsByTagName("Name")
                                ob.arlNames.Add(nodName.InnerText)
                            Next
                            ob.Description = LoadDescription(nodOb, "Description")

                            For Each nodProp As XmlElement In .SelectNodes("Property")
                                Dim prop As New clsProperty
                                Dim sPropKey As String = nodProp.Item("Key").InnerText
                                If Adventure.htblAllProperties.ContainsKey(sPropKey) Then
                                    prop = Adventure.htblAllProperties(sPropKey).Copy
                                    If prop.Type = clsProperty.PropertyTypeEnum.Text Then
                                        prop.StringData = LoadDescription(nodProp, "Value")
                                    ElseIf prop.Type <> clsProperty.PropertyTypeEnum.SelectionOnly Then
                                        prop.Value = nodProp.Item("Value").InnerText
                                    End If
                                    prop.Selected = True
                                    ob.AddProperty(prop)
                                End If
                            Next
                            ob.Location = ob.Location ' Assigns the location object from the object properties                            
                        End With
                        a.htblObjects.Add(ob, ob.Key)
NextOb:
                    Next
                    Debug.WriteLine("End Objects: " & Now)

                    ' Tasks                    
                    For Each nodTask As XmlElement In .SelectNodes("/Adventure/Task")
                        Dim tas As New clsTask
                        With nodTask
                            Dim sKey As String = .Item("Key").InnerText
                            If .Item("Library") IsNot Nothing Then tas.IsLibrary = GetBool(.Item("Library").InnerText)
                            If .Item("ReplaceTask") IsNot Nothing Then tas.ReplaceDuplicateKey = GetBool(.Item("ReplaceTask").InnerText)
                            If a.htblTasks.ContainsKey(sKey) Then
                                If tas.IsLibrary OrElse bLibrary Then
                                    ' We skip loading the task if it is not newer than the one we currently have loaded
                                    ' If there's no timestamp, we have to assume it is newer
                                    If .Item("LastUpdated") IsNot Nothing AndAlso CDate(.Item("LastUpdated").InnerText) <= a.htblTasks(sKey).LastUpdated Then GoTo NextTask
                                    ' If a library item is newer than the library in your game, prompt
                                    Select Case ShouldWeLoadLibraryItem(sKey)
                                        Case LoadItemEum.Yes
                                            a.htblTasks.Remove(sKey)
                                        Case LoadItemEum.No
                                            ' Set the timestamp of the custom version to now, so it's more recent than the "newer" library.  That way we won't be prompted next time
                                            a.htblTasks(sKey).LastUpdated = Now
                                            GoTo NextTask
                                        Case LoadItemEum.Both
                                            ' Keep key, but still add this new one
                                    End Select
                                End If
                                If tas.ReplaceDuplicateKey Then
                                    If a.htblTasks.ContainsKey(sKey) Then a.htblTasks.Remove(sKey)
                                Else
                                    If bAddDuplicateKeys Then
                                        Dim sOldKey As String = sKey
                                        While a.htblTasks.ContainsKey(sKey)
                                            sKey = IncrementKey(sKey)
                                        End While
                                        htblDuplicateKeyMapping.Add(sOldKey, sKey)
                                    Else
                                        GoTo NextTask
                                    End If
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextTask
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            tas.Key = sKey
                            tas.Priority = CInt(.Item("Priority").InnerText)
                            If bLibrary AndAlso Not tas.IsLibrary Then tas.Priority += 50000
                            If bLibrary Then
                                tas.IsLibrary = True
                                tas.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then tas.LastUpdated = GetDate(.Item("LastUpdated").InnerText)
                            tas.Priority = CInt(.Item("Priority").InnerText)
                            If Not .Item("AutoFillPriority") Is Nothing Then tas.AutoFillPriority = CInt(.Item("AutoFillPriority").InnerText)
                            tas.TaskType = EnumParseTaskType(.Item("Type").InnerText)
                            tas.CompletionMessage = LoadDescription(nodTask, "CompletionMessage")
                            Select Case tas.TaskType
                                Case clsTask.TaskTypeEnum.General
                                    For Each nodCommand As XmlElement In .GetElementsByTagName("Command")
                                        ' Simplify Runner so it only has to deal with multiple, or specific refs
                                        tas.arlCommands.Add(FixInitialRefs(nodCommand.InnerText))
                                    Next
                                Case clsTask.TaskTypeEnum.Specific
                                    tas.GeneralKey = .Item("GeneralTask").InnerText
                                    Dim iSpecCount As Integer = 0
                                    ReDim tas.Specifics(-1)
                                    For Each nodSpec As XmlElement In .GetElementsByTagName("Specific")
                                        Dim spec As New clsTask.Specific
                                        iSpecCount += 1
                                        spec.Type = EnumParseSpecificType(nodSpec.Item("Type").InnerText)
                                        spec.Multiple = GetBool(nodSpec.Item("Multiple").InnerText)
                                        For Each nodKey As XmlElement In nodSpec.GetElementsByTagName("Key")
                                            spec.Keys.Add(nodKey.InnerText)
                                        Next
                                        ReDim Preserve tas.Specifics(iSpecCount - 1)
                                        tas.Specifics(iSpecCount - 1) = spec
                                    Next
                                    If .Item("ExecuteParentActions") IsNot Nothing Then ' Old checkbox method
                                        If GetBool(.Item("ExecuteParentActions").InnerText) Then
                                            If tas.CompletionMessage.ToString(True) = "" Then
                                                tas.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeTextAndActions
                                            Else
                                                tas.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeActionsOnly
                                            End If
                                        Else
                                            If tas.CompletionMessage.ToString(True) = "" Then
                                                tas.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.BeforeTextOnly
                                            Else
                                                tas.SpecificOverrideType = clsTask.SpecificOverrideTypeEnum.Override
                                            End If
                                        End If
                                    End If
                                    If .Item("SpecificOverrideType") IsNot Nothing Then
                                        tas.SpecificOverrideType = CType([Enum].Parse(GetType(clsTask.SpecificOverrideTypeEnum), .Item("SpecificOverrideType").InnerText), clsTask.SpecificOverrideTypeEnum)
                                    End If
                                Case clsTask.TaskTypeEnum.System
                                    If .Item("RunImmediately") IsNot Nothing Then tas.RunImmediately = GetBool(.Item("RunImmediately").InnerText)
                                    If .Item("LocationTrigger") IsNot Nothing Then tas.LocationTrigger = .Item("LocationTrigger").InnerText
                            End Select
                            tas.Description = .Item("Description").InnerText
                            tas.Repeatable = GetBool(.Item("Repeatable").InnerText)
                            If .Item("Aggregate") IsNot Nothing Then tas.AggregateOutput = GetBool(.Item("Aggregate").InnerText)
                            If .Item("Continue") IsNot Nothing Then
                                Select Case .Item("Continue").InnerText
                                    Case "ContinueNever", "ContinueOnFail", "ContinueOnNoOutput"
                                        tas.ContinueToExecuteLowerPriority = False
                                    Case "ContinueAlways"
                                        tas.ContinueToExecuteLowerPriority = True
                                End Select
                            End If
                            If .Item("LowPriority") IsNot Nothing Then tas.LowPriority = GetBool(.Item("LowPriority").InnerText)
                            tas.arlRestrictions = LoadRestrictions(nodTask)
                            tas.arlActions = LoadActions(nodTask)
                            tas.FailOverride = LoadDescription(nodTask, "FailOverride")
                            If Not .Item("MessageBeforeOrAfter") Is Nothing Then tas.eDisplayCompletion = EnumParseBeforeAfter(.Item("MessageBeforeOrAfter").InnerText) Else tas.eDisplayCompletion = clsTask.BeforeAfterEnum.Before
                            If .Item("PreventOverriding") IsNot Nothing Then tas.PreventOverriding = GetBool(.Item("PreventOverriding").InnerText)
                        End With
                        a.htblTasks.Add(tas, tas.Key)
                        arlNewTasks.Add(tas.Key)
NextTask:
                    Next
                    Debug.WriteLine("End Tasks: " & Now)


                    For Each nodEvent As XmlElement In .SelectNodes("/Adventure/Event")
                        Dim ev As New clsEvent
                        With nodEvent
                            Dim sKey As String = .Item("Key").InnerText
                            If .Item("Library") IsNot Nothing Then ev.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblEvents.ContainsKey(sKey) Then
                                If a.htblEvents.ContainsKey(sKey) Then
                                    If ev.IsLibrary OrElse bLibrary Then
                                        If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblEvents(sKey).LastUpdated Then GoTo NextEvent
                                        Select Case ShouldWeLoadLibraryItem(sKey)
                                            Case LoadItemEum.Yes
                                                a.htblEvents.Remove(sKey)
                                            Case LoadItemEum.No
                                                GoTo NextEvent
                                            Case LoadItemEum.Both
                                                ' Keep key, but still add this new one
                                        End Select
                                    End If
                                End If
                                If bAddDuplicateKeys Then
                                    While a.htblEvents.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextEvent
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextEvent
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            ev.Key = sKey
                            If bLibrary Then
                                ev.IsLibrary = True
                                ev.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then ev.LastUpdated = CDate(.Item("LastUpdated").InnerText)
                            If .Item("Type") IsNot Nothing Then ev.EventType = CType([Enum].Parse(GetType(clsEvent.EventTypeEnum), .Item("Type").InnerText), clsEvent.EventTypeEnum)
                            ev.Description = .Item("Description").InnerText
                            ev.WhenStart = CType([Enum].Parse(GetType(clsEvent.WhenStartEnum), .Item("WhenStart").InnerText), clsEvent.WhenStartEnum)
                            If .Item("Repeating") IsNot Nothing Then ev.Repeating = GetBool(.Item("Repeating").InnerText)
                            If .Item("RepeatCountdown") IsNot Nothing Then ev.RepeatCountdown = GetBool(.Item("RepeatCountdown").InnerText)

                            Dim sData() As String
                            If .Item("StartDelay") IsNot Nothing Then
                                sData = .Item("StartDelay").InnerText.Split(" "c)
                                ev.StartDelay.iFrom = CInt(sData(0))
                                If sData.Length = 1 Then
                                    ev.StartDelay.iTo = CInt(sData(0))
                                Else
                                    ev.StartDelay.iTo = CInt(sData(2))
                                End If
                            End If
                            sData = .Item("Length").InnerText.Split(" "c)
                            ev.Length.iFrom = CInt(sData(0))
                            If sData.Length = 1 Then
                                ev.Length.iTo = CInt(sData(0))
                            Else
                                ev.Length.iTo = CInt(sData(2))
                            End If

                            For Each nodCtrl As XmlElement In nodEvent.GetElementsByTagName("Control")
                                Dim ctrl As New EventOrWalkControl
                                sData = nodCtrl.InnerText.Split(" "c)
                                ctrl.eControl = CType([Enum].Parse(GetType(EventOrWalkControl.ControlEnum), sData(0)), EventOrWalkControl.ControlEnum)
                                ctrl.eCompleteOrNot = CType([Enum].Parse(GetType(EventOrWalkControl.CompleteOrNotEnum), sData(1)), EventOrWalkControl.CompleteOrNotEnum)
                                ctrl.sTaskKey = sData(2)
                                ReDim Preserve ev.EventControls(ev.EventControls.Length)
                                ev.EventControls(ev.EventControls.Length - 1) = ctrl
                            Next

                            For Each nodSubEvent As XmlElement In nodEvent.GetElementsByTagName("SubEvent")
                                Dim se As New clsEvent.SubEvent(ev.Key)
                                sData = nodSubEvent.Item("When").InnerText.Split(" "c)

                                se.ftTurns.iFrom = CInt(sData(0))
                                If sData.Length = 4 Then
                                    se.ftTurns.iTo = CInt(sData(2))
                                    se.eWhen = CType([Enum].Parse(GetType(clsEvent.SubEvent.WhenEnum), sData(3).ToString), clsEvent.SubEvent.WhenEnum)
                                Else
                                    se.ftTurns.iTo = CInt(sData(0))
                                    se.eWhen = CType([Enum].Parse(GetType(clsEvent.SubEvent.WhenEnum), sData(1).ToString), clsEvent.SubEvent.WhenEnum)
                                End If

                                If nodSubEvent.Item("Action") IsNot Nothing Then
                                    If nodSubEvent.Item("Action").InnerXml.Contains("<Description>") Then
                                        se.oDescription = LoadDescription(nodSubEvent, "Action")
                                    Else
                                        sData = nodSubEvent.Item("Action").InnerText.Split(" "c)
                                        se.eWhat = CType([Enum].Parse(GetType(clsEvent.SubEvent.WhatEnum), sData(0).ToString), clsEvent.SubEvent.WhatEnum)
                                        se.sKey = sData(1)
                                    End If
                                End If

                                If nodSubEvent.Item("Measure") IsNot Nothing Then se.eMeasure = EnumParseSubEventMeasure(nodSubEvent("Measure").InnerText)
                                If nodSubEvent.Item("What") IsNot Nothing Then se.eWhat = EnumParseSubEventWhat(nodSubEvent("What").InnerText)
                                If nodSubEvent.Item("OnlyApplyAt") IsNot Nothing Then se.sKey = nodSubEvent.Item("OnlyApplyAt").InnerText

                                ReDim Preserve ev.SubEvents(ev.SubEvents.Length)
                                ev.SubEvents(ev.SubEvents.Length - 1) = se
                            Next

                        End With

                        a.htblEvents.Add(ev, ev.Key)
NextEvent:
                    Next



                    For Each nodChar As XmlElement In .SelectNodes("/Adventure/Character")
                        Dim chr As New clsCharacter
                        With nodChar
                            Dim sKey As String = .Item("Key").InnerText
                            If Not .Item("Library") Is Nothing Then chr.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblCharacters.ContainsKey(sKey) Then
                                If a.htblCharacters.ContainsKey(sKey) Then
                                    If chr.IsLibrary OrElse bLibrary Then
                                        If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblCharacters(sKey).LastUpdated Then GoTo NextChar
                                        Select Case ShouldWeLoadLibraryItem(sKey)
                                            Case LoadItemEum.Yes
                                                a.htblCharacters.Remove(sKey)
                                            Case LoadItemEum.No
                                                GoTo NextChar
                                            Case LoadItemEum.Both
                                                ' Keep key, but still add this new one
                                        End Select
                                    End If
                                End If
                                If bAddDuplicateKeys Then
                                    While a.htblCharacters.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextChar
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextChar
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            chr.Key = sKey
                            If bLibrary Then
                                chr.IsLibrary = True
                                chr.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then chr.LastUpdated = GetDate(.Item("LastUpdated").InnerText)
                            If Not .Item("Name") Is Nothing Then chr.ProperName = .Item("Name").InnerText
                            If Not .Item("Article") Is Nothing Then chr.Article = .Item("Article").InnerText
                            If Not .Item("Prefix") Is Nothing Then chr.Prefix = .Item("Prefix").InnerText
                            If .Item("Perspective") IsNot Nothing Then chr.Perspective = CType([Enum].Parse(GetType(PerspectiveEnum), .Item("Perspective").InnerText), PerspectiveEnum) Else chr.Perspective = PerspectiveEnum.ThirdPerson
                            If dFileVersion < 5.00002 Then chr.Perspective = PerspectiveEnum.SecondPerson
                            For Each nodName As XmlElement In .GetElementsByTagName("Descriptor")
                                If nodName.InnerText <> "" Then chr.arlDescriptors.Add(nodName.InnerText)
                            Next
                            chr.Description = LoadDescription(nodChar, "Description")
                            For Each nodProp As XmlElement In .SelectNodes("Property")
                                Dim prop As New clsProperty
                                Dim sPropKey As String = nodProp.Item("Key").InnerText
                                If Adventure.htblAllProperties.ContainsKey(sPropKey) Then
                                    prop = Adventure.htblAllProperties(sPropKey).Copy
                                    If prop.Type = clsProperty.PropertyTypeEnum.Text Then
                                        prop.StringData = LoadDescription(nodProp, "Value")
                                    ElseIf prop.Type <> clsProperty.PropertyTypeEnum.SelectionOnly Then
                                        prop.Value = nodProp.Item("Value").InnerText
                                    End If
                                    prop.Selected = True
                                    chr.AddProperty(prop)
                                Else
                                    ErrMsg("Error loading character " & chr.Name & ": Property " & sPropKey & " not found.")
                                End If
                            Next

                            For Each nodWalk As XmlElement In .GetElementsByTagName("Walk")
                                Dim walk As New clsWalk
                                walk.sKey = sKey
                                walk.Description = nodWalk.Item("Description").InnerText
                                walk.Loops = GetBool(nodWalk.Item("Loops").InnerText)
                                walk.StartActive = GetBool(nodWalk.Item("StartActive").InnerText)
                                For Each nodStep As XmlElement In nodWalk.GetElementsByTagName("Step")
                                    Dim [step] As New clsWalk.clsStep
                                    Dim sData() As String = nodStep.InnerText.Split(" "c)
                                    [step].sLocation = sData(0)
                                    [step].ftTurns.iFrom = CInt(sData(1))
                                    If sData.Length = 2 Then
                                        [step].ftTurns.iTo = CInt(sData(1))
                                    Else
                                        [step].ftTurns.iTo = CInt(sData(3))
                                    End If
                                    If dFileVersion < 5.000029 Then
                                        If [step].sLocation = "%Player%" Then
                                            If [step].ftTurns.iFrom < 1 Then [step].ftTurns.iFrom = 1
                                            If [step].ftTurns.iTo < 1 Then [step].ftTurns.iTo = 1
                                        End If
                                    End If
                                    walk.arlSteps.Add([step])
                                Next
                                For Each nodCtrl As XmlElement In nodWalk.GetElementsByTagName("Control")
                                    Dim ctrl As New EventOrWalkControl
                                    Dim sData() As String = nodCtrl.InnerText.Split(" "c)
                                    ctrl.eControl = CType([Enum].Parse(GetType(EventOrWalkControl.ControlEnum), sData(0)), EventOrWalkControl.ControlEnum)
                                    ctrl.eCompleteOrNot = CType([Enum].Parse(GetType(EventOrWalkControl.CompleteOrNotEnum), sData(1)), EventOrWalkControl.CompleteOrNotEnum)
                                    ctrl.sTaskKey = sData(2)
                                    ReDim Preserve walk.WalkControls(walk.WalkControls.Length)
                                    walk.WalkControls(walk.WalkControls.Length - 1) = ctrl
                                Next
                                For Each nodSubWalk As XmlElement In nodWalk.GetElementsByTagName("Activity")
                                    Dim sw As New clsWalk.SubWalk
                                    Dim sData() As String = nodSubWalk.Item("When").InnerText.Split(" "c)
                                    If sData(0) = clsWalk.SubWalk.WhenEnum.ComesAcross.ToString Then
                                        sw.eWhen = clsWalk.SubWalk.WhenEnum.ComesAcross
                                        sw.sKey = sData(1)
                                    Else
                                        sw.ftTurns.iFrom = CInt(sData(0))
                                        If sData.Length = 4 Then
                                            sw.ftTurns.iTo = CInt(sData(2))
                                            sw.eWhen = CType([Enum].Parse(GetType(clsWalk.SubWalk.WhenEnum), sData(3).ToString), clsWalk.SubWalk.WhenEnum)
                                        Else
                                            sw.ftTurns.iTo = CInt(sData(0))
                                            sw.eWhen = CType([Enum].Parse(GetType(clsWalk.SubWalk.WhenEnum), sData(1).ToString), clsWalk.SubWalk.WhenEnum)
                                        End If
                                    End If

                                    If nodSubWalk.Item("Action") IsNot Nothing Then
                                        If nodSubWalk.Item("Action").InnerXml.Contains("<Description>") Then
                                            sw.oDescription = LoadDescription(nodSubWalk, "Action")
                                        Else
                                            sData = nodSubWalk.Item("Action").InnerText.Split(" "c)
                                            sw.eWhat = CType([Enum].Parse(GetType(clsWalk.SubWalk.WhatEnum), sData(0).ToString), clsWalk.SubWalk.WhatEnum)
                                            sw.sKey2 = sData(1)
                                        End If
                                    End If

                                    If nodSubWalk.Item("OnlyApplyAt") IsNot Nothing Then sw.sKey3 = nodSubWalk.Item("OnlyApplyAt").InnerText

                                    ReDim Preserve walk.SubWalks(walk.SubWalks.Length)
                                    walk.SubWalks(walk.SubWalks.Length - 1) = sw
                                Next

                                chr.arlWalks.Add(walk)
                            Next

                            For Each nodTopic As XmlElement In .GetElementsByTagName("Topic")
                                Dim topic As New clsTopic
                                topic.Key = nodTopic.Item("Key").InnerText
                                If nodTopic.Item("ParentKey") IsNot Nothing Then topic.ParentKey = nodTopic.Item("ParentKey").InnerText
                                topic.Summary = nodTopic.Item("Summary").InnerText
                                ' Simplify Runner so it only has to deal with multiple, or specific refs                                
                                topic.Keywords = FixInitialRefs(nodTopic.Item("Keywords").InnerText)

                                topic.oConversation = LoadDescription(nodTopic, "Description")
                                If nodTopic.Item("IsAsk") IsNot Nothing Then topic.bAsk = GetBool(nodTopic.Item("IsAsk").InnerText)
                                If nodTopic.Item("IsCommand") IsNot Nothing Then topic.bCommand = GetBool(nodTopic.Item("IsCommand").InnerText)
                                If nodTopic.Item("IsFarewell") IsNot Nothing Then topic.bFarewell = GetBool(nodTopic.Item("IsFarewell").InnerText)
                                If nodTopic.Item("IsIntro") IsNot Nothing Then topic.bIntroduction = GetBool(nodTopic.Item("IsIntro").InnerText)
                                If nodTopic.Item("IsTell") IsNot Nothing Then topic.bTell = GetBool(nodTopic.Item("IsTell").InnerText)
                                If nodTopic.Item("StayInNode") IsNot Nothing Then topic.bStayInNode = GetBool(nodTopic.Item("StayInNode").InnerText)
                                topic.arlRestrictions = LoadRestrictions(nodTopic)
                                topic.arlActions = LoadActions(nodTopic)
                                chr.htblTopics.Add(topic)
                            Next

                            chr.Location = chr.Location ' Assigns the location object from the character properties
                            chr.CharacterType = EnumParseCharacterType(.Item("Type").InnerText)
                        End With
                        If eLoadWhat = LoadWhatEnum.All Then
                            a.htblCharacters.Add(chr, chr.Key)
                        Else
                            ' Only add the Player character if we don't already have one
                            If chr.CharacterType = clsCharacter.CharacterTypeEnum.NonPlayer OrElse a.Player Is Nothing Then a.htblCharacters.Add(chr, chr.Key)
                        End If
                        For Each sCharFunction As String In New String() {"CharacterName", "DisplayCharacter", "ListHeld", "ListExits", "ListWorn", "LocationOf", "ParentOf", "ProperName"}
                            chr.SearchAndReplace("%" & sCharFunction & "%", "%" & sCharFunction & "[" & chr.Key & "]%")
                        Next
NextChar:
                    Next
                    Debug.WriteLine("End Chars: " & Now)

                End If

                If eLoadWhat = LoadWhatEnum.All OrElse eLoadWhat = LoadWhatEnum.Properies Then
                    For Each nodVar As XmlElement In .SelectNodes("/Adventure/Variable")
                        Dim var As New clsVariable
                        With nodVar
                            Dim sKey As String = .Item("Key").InnerText
                            If Not .Item("Library") Is Nothing Then var.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblVariables.ContainsKey(sKey) Then
                                If var.IsLibrary OrElse bLibrary Then
                                    If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblVariables(sKey).LastUpdated Then GoTo NextVar
                                    Select Case ShouldWeLoadLibraryItem(sKey)
                                        Case LoadItemEum.Yes
                                            a.htblVariables.Remove(sKey)
                                        Case LoadItemEum.No
                                            GoTo NextVar
                                        Case LoadItemEum.Both
                                            ' Keep key, but still add this new one
                                    End Select
                                End If
                                If bAddDuplicateKeys Then
                                    While a.htblVariables.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextVar
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextVar
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            var.Key = sKey
                            If bLibrary Then
                                var.IsLibrary = True
                                var.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then var.LastUpdated = GetDate(.Item("LastUpdated").InnerText)
                            var.Name = .Item("Name").InnerText
                            var.Type = EnumParseVariableType(.Item("Type").InnerText)
                            Dim sValue As String = .Item("InitialValue").InnerText
                            If Not .Item("ArrayLength") Is Nothing Then var.Length = CInt(Val(.Item("ArrayLength").InnerText))
                            If var.Type = clsVariable.VariableTypeEnum.Text OrElse (var.Length > 1 AndAlso sValue.Contains(",")) Then
                                If var.Type = clsVariable.VariableTypeEnum.Numeric Then
                                    Dim iValues() As String = Split(sValue, ",")
                                    For iIndex As Integer = 1 To iValues.Length
                                        var.IntValue(iIndex) = SafeInt(iValues(iIndex - 1))
                                    Next
                                    var.StringValue = sValue
                                Else
                                    For iIndex As Integer = 1 To var.Length
                                        var.StringValue(iIndex) = sValue
                                    Next
                                End If

                            Else
                                For i As Integer = 1 To var.Length
                                    var.IntValue(i) = CInt(Val(sValue))
                                Next i
                            End If
                        End With
                        a.htblVariables.Add(var, var.Key)
NextVar:
                    Next
                End If

                If eLoadWhat = LoadWhatEnum.All OrElse eLoadWhat = LoadWhatEnum.AllExceptProperties Then
                    For Each nodGroup As XmlElement In .SelectNodes("/Adventure/Group")
                        Dim grp As New clsGroup
                        With nodGroup
                            Dim sKey As String = .Item("Key").InnerText
                            If Not .Item("Library") Is Nothing Then grp.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblGroups.ContainsKey(sKey) Then
                                If grp.IsLibrary OrElse bLibrary Then
                                    If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblGroups(sKey).LastUpdated Then GoTo NextGroup
                                    Select Case ShouldWeLoadLibraryItem(sKey)
                                        Case LoadItemEum.Yes
                                            a.htblGroups.Remove(sKey)
                                        Case LoadItemEum.No
                                            GoTo NextGroup
                                        Case LoadItemEum.Both
                                            ' Keep key, but still add this new one
                                    End Select
                                End If
                                If bAddDuplicateKeys Then
                                    While a.htblGroups.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextGroup
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextGroup
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            grp.Key = sKey
                            If bLibrary Then
                                grp.IsLibrary = True
                                grp.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then grp.LastUpdated = GetDate(.Item("LastUpdated").InnerText)
                            grp.Name = .Item("Name").InnerText
                            grp.GroupType = EnumParseGroupType(.Item("Type").InnerText)
                            For Each nodMember As XmlElement In .GetElementsByTagName("Member")
                                grp.arlMembers.Add(nodMember.InnerText)
                            Next
                            For Each nodProp As XmlElement In .GetElementsByTagName("Property")
                                Dim prop As New clsProperty
                                Dim sPropKey As String = nodProp.Item("Key").InnerText
                                If Adventure.htblAllProperties.ContainsKey(sPropKey) Then
                                    prop = Adventure.htblAllProperties(sPropKey).Copy
                                    If nodProp.Item("Value") IsNot Nothing Then
                                        If prop.Type = clsProperty.PropertyTypeEnum.Text Then
                                            prop.StringData = LoadDescription(nodProp, "Value")
                                        ElseIf prop.Type <> clsProperty.PropertyTypeEnum.SelectionOnly Then
                                            prop.Value = nodProp.Item("Value").InnerText
                                        End If
                                    End If
                                    prop.Selected = True
                                    If prop.PropertyOf = grp.GroupType Then
                                        grp.htblProperties.Add(prop)
                                    End If
                                Else
                                    ErrMsg("Error loading group " & grp.Name & ": Property " & sPropKey & " not found.")
                                End If
                            Next
                        End With
                        a.htblGroups.Add(grp, grp.Key)

                        For Each sMember As String In grp.arlMembers
                            Dim itm As clsItemWithProperties = CType(Adventure.GetItemFromKey(sMember), clsItemWithProperties)
                            If itm IsNot Nothing Then itm.ResetInherited() ' In case we've accessed properties, and built inherited before the group existed
                        Next
NextGroup:
                    Next


                    For Each nodALR As XmlElement In .SelectNodes("/Adventure/TextOverride")
                        Dim alr As New clsALR
                        With nodALR
                            Dim sKey As String = .Item("Key").InnerText
                            If .Item("Library") IsNot Nothing Then alr.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblALRs.ContainsKey(sKey) Then
                                If alr.IsLibrary OrElse bLibrary Then
                                    If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblALRs(sKey).LastUpdated Then GoTo NextALR
                                    Select Case ShouldWeLoadLibraryItem(sKey)
                                        Case LoadItemEum.Yes
                                            a.htblALRs.Remove(sKey)
                                        Case LoadItemEum.No
                                            GoTo NextALR
                                        Case LoadItemEum.Both
                                            ' Keep key, but still add this new one
                                    End Select
                                End If
                                If bAddDuplicateKeys Then
                                    While a.htblALRs.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextALR
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextALR
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            alr.Key = sKey
                            If bLibrary Then
                                alr.IsLibrary = True
                                alr.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then alr.LastUpdated = CDate(.Item("LastUpdated").InnerText)
                            alr.OldText = CStr(.Item("OldText").InnerText)
                            alr.NewText = LoadDescription(nodALR, "NewText")
                        End With
                        a.htblALRs.Add(alr, alr.Key)
NextALR:
                    Next

                    For Each nodHint As XmlElement In .SelectNodes("/Adventure/Hint")
                        Dim hint As New clsHint
                        With nodHint
                            Dim sKey As String = .Item("Key").InnerText
                            If .Item("Library") IsNot Nothing Then hint.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblHints.ContainsKey(sKey) Then
                                If hint.IsLibrary OrElse bLibrary Then
                                    If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblHints(sKey).LastUpdated Then GoTo NextHint
                                    Select Case ShouldWeLoadLibraryItem(sKey)
                                        Case LoadItemEum.Yes
                                            a.htblHints.Remove(sKey)
                                        Case LoadItemEum.No
                                            GoTo NextHint
                                        Case LoadItemEum.Both
                                            ' Keep key, but still add this new one
                                    End Select
                                End If
                                If bAddDuplicateKeys Then
                                    While a.htblHints.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextHint
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextHint
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            hint.Key = sKey
                            If bLibrary Then
                                hint.IsLibrary = True
                                hint.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then hint.LastUpdated = CDate(.Item("LastUpdated").InnerText)
                            hint.Question = CStr(.Item("Question").InnerText)
                            hint.SubtleHint = LoadDescription(nodHint, "Subtle")
                            hint.SledgeHammerHint = LoadDescription(nodHint, "Sledgehammer")
                            hint.arlRestrictions = LoadRestrictions(nodHint)
                        End With
                        a.htblHints.Add(hint, hint.Key)
NextHint:
                    Next

                    For Each nodSynonym As XmlElement In .SelectNodes("/Adventure/Synonym")
                        Dim synonym As New clsSynonym
                        With nodSynonym
                            Dim sKey As String = .Item("Key").InnerText
                            If .Item("Library") IsNot Nothing Then synonym.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblSynonyms.ContainsKey(sKey) Then
                                If synonym.IsLibrary OrElse bLibrary Then
                                    If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblSynonyms(sKey).LastUpdated Then GoTo NextSynonym
                                    Select Case ShouldWeLoadLibraryItem(sKey)
                                        Case LoadItemEum.Yes
                                            a.htblSynonyms.Remove(sKey)
                                        Case LoadItemEum.No
                                            GoTo NextSynonym
                                        Case LoadItemEum.Both
                                            ' Keep key, but still add this new one
                                    End Select
                                End If

                                If bAddDuplicateKeys Then
                                    While a.htblSynonyms.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextSynonym
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextSynonym
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            synonym.Key = sKey
                            If bLibrary Then
                                synonym.IsLibrary = True
                                synonym.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then synonym.LastUpdated = CDate(.Item("LastUpdated").InnerText)

                            For Each nodFrom As XmlElement In .GetElementsByTagName("From")
                                synonym.ChangeFrom.Add(nodFrom.InnerText)
                            Next
                            synonym.ChangeTo = .Item("To").InnerText
                        End With
                        a.htblSynonyms.Add(synonym)
NextSynonym:
                    Next


                    For Each nodUDF As XmlElement In .SelectNodes("/Adventure/Function")
                        Dim udf As New clsUserFunction
                        With nodUDF
                            Dim sKey As String = .Item("Key").InnerText
                            If .Item("Library") IsNot Nothing Then udf.IsLibrary = GetBool(.Item("Library").InnerText)
                            If a.htblUDFs.ContainsKey(sKey) Then
                                If udf.IsLibrary OrElse bLibrary Then
                                    If .Item("LastUpdated") Is Nothing OrElse CDate(.Item("LastUpdated").InnerText) <= a.htblUDFs(sKey).LastUpdated Then GoTo NextUDF
                                    Select Case ShouldWeLoadLibraryItem(sKey)
                                        Case LoadItemEum.Yes
                                            a.htblUDFs.Remove(sKey)
                                        Case LoadItemEum.No
                                            GoTo NextUDF
                                        Case LoadItemEum.Both
                                            ' Keep key, but still add this new one
                                    End Select
                                End If
                                If bAddDuplicateKeys Then
                                    While a.htblUDFs.ContainsKey(sKey)
                                        sKey = IncrementKey(sKey)
                                    End While
                                Else
                                    GoTo NextUDF
                                End If
                            ElseIf bLibrary AndAlso ShouldWeLoadLibraryItem(sKey) = LoadItemEum.No Then
                                GoTo NextUDF
                            End If
                            If a.listExcludedItems.Contains(sKey) Then a.listExcludedItems.Remove(sKey)
                            udf.Key = sKey
                            If bLibrary Then
                                udf.IsLibrary = True
                                udf.LoadedFromLibrary = True
                            End If
                            If Not .Item("LastUpdated") Is Nothing Then udf.LastUpdated = CDate(.Item("LastUpdated").InnerText)

                            udf.Name = CStr(.Item("Name").InnerText)
                            udf.Output = LoadDescription(nodUDF, "Output")
                            For Each nodArgument As XmlElement In .GetElementsByTagName("Argument")
                                With nodArgument
                                    Dim arg As New clsUserFunction.Argument
                                    arg.Name = .Item("Name").InnerText
                                    arg.Type = CType([Enum].Parse(GetType(clsUserFunction.ArgumentType), .Item("Type").InnerText), clsUserFunction.ArgumentType)
                                    udf.Arguments.Add(arg)
                                End With
                            Next

                        End With
                        a.htblUDFs.Add(udf, udf.Key)
NextUDF:
                    Next

                    If Not bLibrary Then
                        For Each nodExclude As XmlElement In .SelectNodes("/Adventure/Exclude")
                            a.listExcludedItems.Add(nodExclude.InnerText)
                        Next
                    End If

                    If Not bLibrary Then
                        If .Item("Map") IsNot Nothing Then
                            Adventure.Map.Pages.Clear()
                            For Each nodPage As XmlElement In .SelectNodes("/Adventure/Map/Page")
                                With nodPage
                                    Dim sPageKey As String = .Item("Key").InnerText
                                    If IsNumeric(sPageKey) Then
                                        Dim page As New MapPage(SafeInt(sPageKey))
                                        If .Item("Label") IsNot Nothing Then page.Label = .Item("Label").InnerText
                                        If .Item("Selected") IsNot Nothing AndAlso GetBool(.Item("Selected").InnerText) Then Adventure.Map.SelectedPage = sPageKey
                                        For Each nodNode As XmlElement In .GetElementsByTagName("Node")
                                            With nodNode
                                                Dim node As New MapNode
                                                If .Item("Key") IsNot Nothing Then node.Key = .Item("Key").InnerText
                                                Dim loc As clsLocation = Adventure.htblLocations(node.Key)
                                                If loc IsNot Nothing Then
                                                    node.Text = loc.ShortDescriptionSafe ' StripCarats(ReplaceALRs(loc.ShortDescription.ToString))
                                                    If .Item("X") IsNot Nothing Then node.Location.X = SafeInt(.Item("X").InnerText)
                                                    If .Item("Y") IsNot Nothing Then node.Location.Y = SafeInt(.Item("Y").InnerText)
                                                    If .Item("Z") IsNot Nothing Then node.Location.Z = SafeInt(.Item("Z").InnerText)
                                                    If .Item("Height") IsNot Nothing Then node.Height = SafeInt(.Item("Height").InnerText) Else node.Height = 4
                                                    If .Item("Width") IsNot Nothing Then node.Width = SafeInt(.Item("Width").InnerText) Else node.Width = 6

                                                    If loc.arlDirections(DirectionsEnum.Up).LocationKey IsNot Nothing Then node.bHasUp = True
                                                    If loc.arlDirections(DirectionsEnum.Down).LocationKey IsNot Nothing Then node.bHasDown = True
                                                    If loc.arlDirections(DirectionsEnum.In).LocationKey IsNot Nothing Then node.bHasIn = True
                                                    If loc.arlDirections(DirectionsEnum.Out).LocationKey IsNot Nothing Then node.bHasOut = True

                                                    For Each nodLink As XmlElement In .GetElementsByTagName("Link")
                                                        With nodLink
                                                            Dim link As New MapLink
                                                            link.sSource = node.Key
                                                            If .Item("SourceAnchor") IsNot Nothing Then link.eSourceLinkPoint = CType([Enum].Parse(GetType(DirectionsEnum), .Item("SourceAnchor").InnerText), DirectionsEnum)
                                                            link.sDestination = loc.arlDirections(link.eSourceLinkPoint).LocationKey
                                                            If Adventure.Map.DottedLink(loc.arlDirections(link.eSourceLinkPoint)) Then
                                                                link.Style = DashStyles.Dot
                                                            Else
                                                                link.Style = DashStyles.Solid
                                                            End If
                                                            If .Item("DestinationAnchor") IsNot Nothing Then link.eDestinationLinkPoint = CType([Enum].Parse(GetType(DirectionsEnum), .Item("DestinationAnchor").InnerText), DirectionsEnum)
                                                            Dim sDest As String = loc.arlDirections(link.eSourceLinkPoint).LocationKey
                                                            If sDest IsNot Nothing AndAlso Adventure.htblLocations.ContainsKey(sDest) Then
                                                                Dim locDest As clsLocation = Adventure.htblLocations(sDest)
                                                                If locDest IsNot Nothing Then
                                                                    If locDest.arlDirections(link.eDestinationLinkPoint).LocationKey = loc.Key Then
                                                                        link.Duplex = True
                                                                        If Adventure.Map.DottedLink(locDest.arlDirections(link.eDestinationLinkPoint)) Then link.Style = DashStyles.Dot
                                                                    End If
                                                                End If
                                                            End If
                                                            For Each nodAnchor As XmlElement In .GetElementsByTagName("Anchor")
                                                                Dim p As New Point3D
                                                                With nodAnchor
                                                                    If .Item("X") IsNot Nothing Then p.X = SafeInt(.Item("X").InnerText)
                                                                    If .Item("Y") IsNot Nothing Then p.Y = SafeInt(.Item("Y").InnerText)
                                                                    If .Item("Z") IsNot Nothing Then p.Z = SafeInt(.Item("Z").InnerText)
                                                                End With
                                                                ReDim Preserve link.OrigMidPoints(link.OrigMidPoints.Length)
                                                                link.OrigMidPoints(link.OrigMidPoints.Length - 1) = p
                                                                Dim ar As New Anchor
                                                                ar.Visible = True
                                                                ar.Parent = link
                                                                link.Anchors.Add(ar)
                                                            Next
                                                            node.Links.Add(link.eSourceLinkPoint, link)
                                                        End With
                                                    Next

                                                    node.Page = page.iKey
                                                    page.AddNode(node)
                                                End If
                                            End With
                                        Next
                                        Adventure.Map.Pages.Add(page.iKey, page)
                                        Adventure.Map.Pages(page.iKey).SortNodes()
                                    End If
                                End With
                            Next
                        End If
                    End If

                    ' Now fix any remapped keys
                    ' This must only remap our newly imported tasks, not all the original ones!
                    '
                    For Each sOldKey As String In htblDuplicateKeyMapping.Keys
                        For Each sTask As String In arlNewTasks
                            Dim tas As clsTask = a.htblTasks(sTask)
                            If tas.GeneralKey = sOldKey Then tas.GeneralKey = htblDuplicateKeyMapping(sOldKey)
                            For Each act As clsAction In tas.arlActions
                                If act.sKey1 = sOldKey Then act.sKey1 = htblDuplicateKeyMapping(sOldKey)
                                If act.sKey2 = sOldKey Then act.sKey2 = htblDuplicateKeyMapping(sOldKey)
                            Next
                        Next
                    Next

                    Adventure.BlorbMappings.Clear()
                    If .Item("FileMappings") IsNot Nothing Then
                        For Each nodMapping As XmlElement In .SelectNodes("/Adventure/FileMappings/Mapping")
                            With nodMapping
                                Dim iResource As Integer = SafeInt(.Item("Resource").InnerText)
                                Dim sFile As String = .Item("File").InnerText
                                Adventure.BlorbMappings.Add(sFile, iResource)
                            End With
                        Next
                    End If

                End If

            End With

            ' Correct any old style functions
            ' Player.Held.Weight > Player.Held.Weight.Sum
            If Not bLibrary AndAlso dFileVersion < 5.0000311 Then
                For Each sd As SingleDescription In Adventure.AllDescriptions
                    For Each p As clsProperty In Adventure.htblAllProperties.Values
                        If p.Type = clsProperty.PropertyTypeEnum.Integer OrElse p.Type = clsProperty.PropertyTypeEnum.ValueList Then
                            If sd.Description.Contains("." & p.Key) Then
                                sd.Description = sd.Description.Replace("." & p.Key, "." & p.Key & ".Sum")
                            End If
                        End If
                    Next
                Next
            End If


            If eLoadWhat = LoadWhatEnum.All AndAlso iCorrectedTasks > 0 Then
                Glue.ShowInfo(iCorrectedTasks & " tasks have been updated.", "Adventure Upgrade")
            End If

            With Adventure
                If .Map.Pages.Count = 1 AndAlso .Map.Pages.ContainsKey(0) AndAlso .Map.Pages(0).Nodes.Count = 0 Then
                    .Map = New clsMap
                    .Map.RecalculateLayout()
                End If
            End With
            dFileVersion = dVersion ' Set back to current version so copy/paste etc has correct versions

            Return True

        Catch exLAE As LoadAbortException
            ' Ignore
            Return False
        Catch exXML As XmlException
            If bLibrary Then
                ErrMsg("The file '" & sFilename & "' you are trying to load is not a valid ADRIFT Module.", exXML)
            Else
                ErrMsg("Error loading Adventure", exXML)
            End If
        Catch ex As Exception
            If ex.Message.Contains("Root element is missing") Then
                ErrMsg("The file you are trying to load is not a valid ADRIFT v5.0 file.")
            Else
                ErrMsg("Error loading Adventure", ex)
            End If
            Return False
        End Try

    End Function

    Friend Sub CreateMandatoryProperties()
        For Each sKey As String In New String() {OBJECTARTICLE, OBJECTPREFIX, OBJECTNOUN}
            If Not Adventure.htblObjectProperties.ContainsKey(sKey) Then
                Dim prop As New clsProperty
                With prop
                    .Key = sKey
                    Select Case .Key
                        Case OBJECTARTICLE
                            .Description = "Object Article"
                        Case OBJECTPREFIX
                            .Description = "Object Prefix"
                        Case OBJECTNOUN
                            .Description = "Object Name"
                    End Select
                    .PropertyOf = clsProperty.PropertyOfEnum.Objects
                    .Type = clsProperty.PropertyTypeEnum.Text
                End With
                Adventure.htblAllProperties.Add(prop)
            End If
            Adventure.htblObjectProperties(sKey).GroupOnly = True
        Next

        If Not Adventure.htblLocationProperties.ContainsKey(SHORTLOCATIONDESCRIPTION) Then
            Dim prop As New clsProperty
            With prop
                .Key = SHORTLOCATIONDESCRIPTION
                .Description = "Short Location Description"
                .PropertyOf = clsProperty.PropertyOfEnum.Locations
                .Type = clsProperty.PropertyTypeEnum.Text
            End With
            Adventure.htblAllProperties.Add(prop)
        End If
        Adventure.htblLocationProperties(SHORTLOCATIONDESCRIPTION).GroupOnly = True

        If Not Adventure.htblLocationProperties.ContainsKey(LONGLOCATIONDESCRIPTION) Then
            Dim prop As New clsProperty
            With prop
                .Key = LONGLOCATIONDESCRIPTION
                .Description = "Long Location Description"
                .PropertyOf = clsProperty.PropertyOfEnum.Locations
                .Type = clsProperty.PropertyTypeEnum.Text
            End With
            Adventure.htblAllProperties.Add(prop)
        End If
        Adventure.htblLocationProperties(LONGLOCATIONDESCRIPTION).GroupOnly = True

        If Not Adventure.htblCharacterProperties.ContainsKey(CHARACTERPROPERNAME) Then
            Dim prop As New clsProperty
            With prop
                .Key = CHARACTERPROPERNAME
                .Description = "Character Proper Name"
                .PropertyOf = clsProperty.PropertyOfEnum.Characters
                .Type = clsProperty.PropertyTypeEnum.Text
            End With
            Adventure.htblAllProperties.Add(prop)
        End If
        Adventure.htblCharacterProperties(CHARACTERPROPERNAME).GroupOnly = True

        If Not Adventure.htblObjectProperties.ContainsKey("StaticOrDynamic") Then
            Dim prop As New clsProperty
            With prop
                .Key = "StaticOrDynamic"
                .Description = "Object type"
                .Mandatory = True
                .PropertyOf = clsProperty.PropertyOfEnum.Objects
                .Type = clsProperty.PropertyTypeEnum.StateList
                .arlStates.Add("Static")
                .arlStates.Add("Dynamic")
            End With
            Adventure.htblAllProperties.Add(prop)
        End If
        Adventure.htblObjectProperties("StaticOrDynamic").GroupOnly = True

        If Not Adventure.htblObjectProperties.ContainsKey("StaticLocation") Then
            Dim prop As New clsProperty
            With prop
                .Key = "StaticLocation"
                .Description = "Location of the object"
                .Mandatory = True
                .PropertyOf = clsProperty.PropertyOfEnum.Objects
                .Type = clsProperty.PropertyTypeEnum.StateList
                .arlStates.Add("Hidden")
                .arlStates.Add("Single Location")
                .arlStates.Add("Location Group")
                .arlStates.Add("Everywhere")
                .arlStates.Add("Part of Character")
                .arlStates.Add("Part of Object")
                .DependentKey = "StaticOrDynamic"
                .DependentValue = "Static"
            End With
            Adventure.htblAllProperties.Add(prop)
        End If
        Adventure.htblObjectProperties("StaticLocation").GroupOnly = True

        If Not Adventure.htblObjectProperties.ContainsKey("DynamicLocation") Then
            Dim prop As New clsProperty
            With prop
                .Key = "DynamicLocation"
                .Description = "Location of the object"
                .Mandatory = True
                .PropertyOf = clsProperty.PropertyOfEnum.Objects
                .Type = clsProperty.PropertyTypeEnum.StateList
                .arlStates.Add("Hidden")
                .arlStates.Add("Held by Character")
                .arlStates.Add("Worn by Character")
                .arlStates.Add("In Location")
                .arlStates.Add("Inside Object")
                .arlStates.Add("On Object")
                .DependentKey = "StaticOrDynamic"
                .DependentValue = "Dynamic"
            End With
            Adventure.htblAllProperties.Add(prop)
        End If
        Adventure.htblObjectProperties("DynamicLocation").GroupOnly = True

        For Each sProp As String In New String() {"AtLocation", "AtLocationGroup", "PartOfWhat", "PartOfWho", "HeldByWho", "WornByWho", "InLocation", "InsideWhat", "OnWhat"}
            If Adventure.htblObjectProperties.ContainsKey(sProp) Then Adventure.htblObjectProperties(sProp).GroupOnly = True
        Next

    End Sub

    ' Simple encryption
    Function Dencode(ByVal sText As String) As String

        Rnd(-1)
        Randomize(1976)

        Dencode = ""
        For n As Integer = 1 To sText.Length
            Dencode = Dencode & Chr((Asc(Mid(sText, n, 1)) Xor Int(CInt(Rnd() * 255 - 0.5))) Mod 256)
        Next n

    End Function

    Friend Function Dencode(ByVal sText As String, ByVal lOffset As Long) As Byte() ' String
        Rnd(-1)
        Randomize(1976)

        For n As Long = 1 To lOffset - 1
            Rnd()
        Next

        Dim result(sText.Length - 1) As Byte
        For n As Integer = 1 To sText.Length
            result(n - 1) = CByte((Asc(Mid(sText, n, 1)) Xor Int(CInt(Rnd() * 255 - 0.5))) Mod 256)
        Next

        Return result
    End Function

    Friend Function Dencode(ByVal bData As Byte(), ByVal lOffset As Long) As Byte() ' String
        Rnd(-1)
        Randomize(1976)

        For n As Long = 1 To lOffset - 1
            Rnd()
        Next

        Dim result(bData.Length - 1) As Byte
        For n As Integer = 1 To bData.Length
            'result(n - 1) = CByte((Asc(CChar(sMid(sText, n, 1))) Xor Int(CInt(Rnd() * 255 - 0.5))) Mod 256)
            result(n - 1) = CByte((bData(n - 1) Xor Int(CInt(Rnd() * 255 - 0.5))) Mod 256)
        Next

        Return result
    End Function

End Module