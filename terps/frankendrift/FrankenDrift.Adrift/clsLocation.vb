#If Adravalon Then
Imports FrankenDrift.Glue
Imports FrankenDrift.Glue.Util
#End If

Public Class clsDirection
    Public LocationKey As String
    Friend Restrictions As New RestrictionArrayList
    Friend bEverBeenBlocked As Boolean = False
End Class

Public Class clsLocation
    Inherits clsItemWithProperties

    Private oShortDesc As Description
    Private oLongDesc As Description
    Public arlDirections As New DirectionArrayList
    Public MapNode As MapNode

    ' For displaying in Listboxes
    Public Overrides Function ToString() As String
        Return Me.ShortDescription.ToString
    End Function

    Protected Overrides ReadOnly Property PropertyGroupType() As clsGroup.GroupTypeEnum
        Get
            Return clsGroup.GroupTypeEnum.Locations
        End Get
    End Property

    Private _HideOnMap As Boolean
    Public Property HideOnMap As Boolean
        Get
            Return _HideOnMap
        End Get
        Set(value As Boolean)
            _HideOnMap = value
        End Set
    End Property

    Public Property SeenBy(ByVal sCharKey As String) As Boolean
        Get
            Return Adventure.htblCharacters(sCharKey).HasSeenLocation(Key)
        End Get
        Set(ByVal Value As Boolean)
            Adventure.htblCharacters(sCharKey).HasSeenLocation(Key) = Value
        End Set
    End Property


    Friend ReadOnly Property ShortDescriptionSafe As String
        Get
            If Adventure.dVersion < 5 Then
                ' v4 didn't replace ALRs on statusbar and map
                Return StripCarats(ReplaceFunctions(ShortDescription.ToString))
            Else
                Return StripCarats(ReplaceALRs(ShortDescription.ToString))
            End If
        End Get
    End Property

    Friend Property ShortDescription() As Description
        Get
            If HasProperty(SHORTLOCATIONDESCRIPTION) Then
                Dim descShortDesc As Description = oShortDesc.Copy
                For Each sd As SingleDescription In GetProperty(SHORTLOCATIONDESCRIPTION).StringData
                    descShortDesc.Add(sd)
                Next
                Return descShortDesc
            Else
                    Return oShortDesc
                End If
        End Get
        Set(ByVal Value As Description)
            oShortDesc = Value
        End Set
    End Property

    Friend Property LongDescription() As Description
        Get
            If HasProperty(LONGLOCATIONDESCRIPTION) Then
                Dim descLongDesc As Description = oLongDesc.Copy
                For Each sd As SingleDescription In GetProperty(LONGLOCATIONDESCRIPTION).StringData
                    descLongDesc.Add(sd)
                Next
                Return descLongDesc
            Else
                    Return oLongDesc
                End If
        End Get
        Set(ByVal Value As Description)
            oLongDesc = Value
        End Set
    End Property


    Public ReadOnly Property sRegularExpressionString(Optional ByVal bMyRE As Boolean = False) As String
        Get
            If bMyRE Then
                ' The ADRIFT 'advanced command construction' expression
                Dim sRE As String = ""
                For Each sWord As String In Split(ShortDescription.ToString(True), " ")
                    If sWord <> "" Then sRE &= "{" & sWord.ToLower & "} "
                Next
                Return sRE
            Else
                ' Real Regular Expressions            
                Dim sRE As String = ""
                For Each sWord As String In Split(ShortDescription.ToString(True), " ")
                    If sRE <> "" Then sRE &= "( )?"
                    If sWord <> "" Then sRE &= "(" & sWord.ToLower & ")?"
                Next
                Return sRE
            End If

        End Get
    End Property


    Public ReadOnly Property ViewLocation() As String
        Get
            Dim sView As String = LongDescription.ToString

            ' Do any specific listed objects
            Dim htblObjects As ObjectHashTable = ObjectsInLocation(WhichObjectsToListEnum.AllSpecialListedObjects, True)
            For Each ob As clsObject In htblObjects.Values
                pSpace(sView)
                sView &= ob.ListDescription
            Next

            htblObjects = ObjectsInLocation(WhichObjectsToListEnum.AllGeneralListedObjects, True)
            If htblObjects.Count > 0 Then
                If sView <> "" OrElse Adventure.dVersion < 5 Then
                    pSpace(sView)
                    If Adventure.dVersion < 5 AndAlso sView = "" Then sView = "  "
                    sView &= "Also here is " & htblObjects.List(, , ArticleTypeEnum.Indefinite) & "."
                Else
                    sView &= "There is " & htblObjects.List(, , ArticleTypeEnum.Indefinite) & " here."
                End If
            End If

            For Each e As clsEvent In Adventure.htblEvents.Values
                Dim sLookText As String = e.LookText()
                If sLookText <> "" Then sView = pSpace(sView) & sLookText
            Next

            Dim dCharDesc As New Dictionary(Of String, StringArrayList) ' Description without name, list of names
            For Each sKey As String In CharactersVisibleAtLocation.Keys
                Dim ch As clsCharacter = Adventure.htblCharacters(sKey)
                Dim sName As String = ch.Name(, False)
                Dim sIsHereDesc As String
                ' Default to Char is here unless we have property (which can set value to blank)
                If ch.HasProperty("CharHereDesc") Then
                    sIsHereDesc = ReplaceFunctions(ch.IsHereDesc)
                Else
                    sIsHereDesc = sName & " is here."
                End If
                If sIsHereDesc <> "" Then
                    Dim sDescWithoutName As String = ReplaceIgnoreCase(sIsHereDesc, sName, "##CHARNAME##")
                    If Not dCharDesc.ContainsKey(sDescWithoutName) Then dCharDesc.Add(sDescWithoutName, New StringArrayList)
                    If Not dCharDesc(sDescWithoutName).Contains(sName) Then dCharDesc(sDescWithoutName).Add(sName)
                End If
            Next
            For Each sDesc As String In dCharDesc.Keys
                With dCharDesc(sDesc)
                    pSpace(sView)
                    If .Count > 1 Then
                        sDesc = sDesc.Replace(" is ", " are ")
                    End If
                    sView &= sDesc.Replace("##CHARNAME##", .List)
                End With
            Next

            If Adventure.ShowExits Then
                Dim iExitCount As Integer = 0
                Dim sListExists As String = Adventure.Player.ListExits(Me.Key, iExitCount)
                pSpace(sView)
                If iExitCount > 1 Then
                    sView &= "Exits are " & sListExists & "."
                ElseIf iExitCount = 1 Then
                    sView &= "An exit leads " & sListExists & "."
                End If
            End If

            If sView = "" Then sView = "Nothing special."

            If UserSession.bShowShortLocations Then
                sView = "<b>" & ShortDescription.ToString & "</b>" & vbCrLf & sView
                If Adventure.dVersion >= 5 Then sView = vbCrLf & sView
            End If
            Return sView
        End Get
    End Property

    Public Sub New()

        For dir As DirectionsEnum = DirectionsEnum.North To DirectionsEnum.NorthWest
            Dim dirstruct As New clsDirection
            arlDirections.Add(dirstruct)
        Next
        oLongDesc = New Description
        oShortDesc = New Description

    End Sub

    Friend Enum WhichObjectsToListEnum
        AllObjects
        AllListedObjects
        AllGeneralListedObjects
        AllSpecialListedObjects
    End Enum
    ' Directly means they have to be directly in the room, i.e. not held by a character etc
    Friend ReadOnly Property ObjectsInLocation(Optional ByVal ListWhat As WhichObjectsToListEnum = WhichObjectsToListEnum.AllListedObjects, Optional ByVal bDirectly As Boolean = True) As ObjectHashTable
        Get
            Dim htblObjectsInLocation As New ObjectHashTable

            For Each ob As clsObject In Adventure.htblObjects.Values
                If ob.ExistsAtLocation(Key, bDirectly) Then
                    Select Case ListWhat
                        Case WhichObjectsToListEnum.AllGeneralListedObjects ' Dynamic objects not excluded plus static objects explicitly included, unless specially listed
                            If ((Not ob.IsStatic AndAlso Not ob.ExplicitlyExclude) OrElse (ob.IsStatic AndAlso ob.ExplicitlyList)) Then
                                If ob.ListDescription = "" Then htblObjectsInLocation.Add(ob, ob.Key)
                            End If
                        Case WhichObjectsToListEnum.AllListedObjects ' All listed objects, including special listed
                            If ((Not ob.IsStatic AndAlso Not ob.ExplicitlyExclude) OrElse (ob.IsStatic AndAlso ob.ExplicitlyList)) Then
                                htblObjectsInLocation.Add(ob, ob.Key)
                            End If
                        Case WhichObjectsToListEnum.AllObjects ' Any object in the location, whether indirectly or not
                            htblObjectsInLocation.Add(ob, ob.Key)
                        Case WhichObjectsToListEnum.AllSpecialListedObjects ' Specially listed objects only (i.e. they have a special listing description)
                            If ((Not ob.IsStatic AndAlso Not ob.ExplicitlyExclude) OrElse (ob.IsStatic AndAlso ob.ExplicitlyList)) Then
                                If ob.ListDescription <> "" Then htblObjectsInLocation.Add(ob, ob.Key)
                            End If
                    End Select
                End If
            Next

            Return htblObjectsInLocation
        End Get
    End Property

    ' Characters directly in the location
    Friend ReadOnly Property CharactersDirectlyInLocation(Optional ByVal bIncludePlayer As Boolean = False) As CharacterHashTable
        Get
            Dim htblCharactersInLocation As New CharacterHashTable

            For Each ch As clsCharacter In Adventure.htblCharacters.Values
                If ch.Location.Key = Me.Key AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation AndAlso (bIncludePlayer OrElse ch.Key <> Adventure.Player.Key) Then htblCharactersInLocation.Add(ch, ch.Key) ' AndAlso ch.Key <> Adventure.Player.Key
            Next

            Return htblCharactersInLocation

        End Get
    End Property

    ' Characters visible in location (can be in open objects, on objects etc)
    Friend ReadOnly Property CharactersVisibleAtLocation(Optional ByVal bIncludePlayer As Boolean = False) As CharacterHashTable
        Get
            Dim htblCharactersInLocation As New CharacterHashTable

            'Dim sLoc As String = Adventure.Player.Location.LocationKey
            For Each ch As clsCharacter In Adventure.htblCharacters.Values
                If ch IsNot Adventure.Player AndAlso ch.IsVisibleAtLocation(Me.Key) Then htblCharactersInLocation.Add(ch, ch.Key)
            Next

            Return htblCharactersInLocation
        End Get
    End Property

    Public Overrides ReadOnly Property Clone() As clsItem
        Get
            Return CType(Me.MemberwiseClone, clsLocation)
        End Get
    End Property

    Public Function IsAdjacent(ByVal sKey As String) As Boolean

        For Each dir As clsDirection In Me.arlDirections
            If dir.LocationKey = sKey Then Return True
        Next
        Return False

    End Function

    Public Function DirectionTo(ByVal sKey As String) As String

        If sKey = Me.Key Then Return "not moved"

        For drn As SharedModule.DirectionsEnum = DirectionsEnum.North To DirectionsEnum.NorthWest
            If arlDirections(drn).LocationKey = sKey Then
                Select Case drn
                    Case DirectionsEnum.North, DirectionsEnum.East, DirectionsEnum.South, DirectionsEnum.West
                        Return "the " & drn.ToString.ToLower
                    Case DirectionsEnum.Up
                        Return "above"
                    Case DirectionsEnum.Down
                        Return "below"
                    Case DirectionsEnum.In
                        Return "inside"
                    Case DirectionsEnum.Out
                        Return "outside"
                    Case DirectionsEnum.NorthEast
                        Return "the north-east"
                    Case DirectionsEnum.NorthWest
                        Return "the north-west"
                    Case DirectionsEnum.SouthEast
                        Return "the south-east"
                    Case DirectionsEnum.SouthWest
                        Return "the south-west"
                End Select
            End If
        Next

        Return "nowhere"

    End Function
    Public Overrides ReadOnly Property CommonName() As String
        Get
            Return ShortDescription.ToString
        End Get
    End Property
    Friend Overrides ReadOnly Property AllDescriptions() As System.Collections.Generic.List(Of SharedModule.Description)
        Get
            Dim all As New Generic.List(Of Description)
            all.Add(Me.ShortDescription)
            all.Add(Me.LongDescription)
            For Each p As clsProperty In htblActualProperties.Values
                all.Add(p.StringData)
            Next
            Return all
        End Get
    End Property
    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object
        Dim iCount As Integer = iReplacements
        '
        Return iReplacements - iCount
    End Function

    Public Overrides Function ReferencesKey(ByVal sKey As String) As Integer
        Dim iCount As Integer = 0
        For Each d As Description In AllDescriptions
            iCount += d.ReferencesKey(sKey)
        Next
        For Each d As clsDirection In arlDirections
            If d.LocationKey = sKey Then iCount += 1
            iCount += d.Restrictions.ReferencesKey(sKey)
        Next
        iCount += htblActualProperties.ReferencesKey(sKey)

        Return iCount
    End Function


    Public Overrides Function DeleteKey(ByVal sKey As String) As Boolean
        For Each d As Description In AllDescriptions
            If Not d.DeleteKey(sKey) Then Return False
        Next
        For Each d As clsDirection In arlDirections
            If d.LocationKey = sKey Then
                d.Restrictions.Clear()
                d.LocationKey = ""
            Else
                If Not d.Restrictions.DeleteKey(sKey) Then Return False
            End If
        Next
        If Not htblActualProperties.DeleteKey(sKey) Then Return False

        Return True
    End Function

    Friend Overrides ReadOnly Property Parent As String
        Get
            Return "" ' Suppose we could return a Location Group, but what if we were member of more than one?
        End Get
    End Property

End Class
