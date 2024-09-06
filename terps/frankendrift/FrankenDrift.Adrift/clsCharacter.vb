#If Adravalon Then
Imports FrankenDrift.Glue
#End If

<DebuggerDisplay("{Name}")>
Public Class clsCharacter
    Inherits clsItemWithProperties
    Private cLocation As clsCharacterLocation
    Private iMaxSize As Integer
    Private iMaxWeight As Integer
    Private eCharacterType As CharacterTypeEnum = CharacterTypeEnum.NonPlayer
    Private oDescription As Description
    Private sIsHereDesc As String

    Private sName As String
    Private sArticle As String
    Private sPrefix As String
    Friend arlDescriptors As New StringArrayList
    Friend arlWalks As New WalkArrayList

    Private htblSeenLocation As New BooleanHashTable
    Private htblSeenObject As New BooleanHashTable
    Private htblSeenChar As New BooleanHashTable

    Friend htblTopics As New TopicHashTable


    Public Sub New()
        Location = New clsCharacterLocation(Me)
    End Sub


    Public Overrides ReadOnly Property CommonName() As String
        Get
            Return Name
        End Get
    End Property


    ' Has the player been told the character exists (e.g. "a man").  Once introduced, they will be referred to as "the man"
    ' until they are no longer in the same location.
    '
    Private bIntroduced As Boolean = False
    Friend Property Introduced As Boolean
        Get
            Return bIntroduced
        End Get
        Set(value As Boolean)
            bIntroduced = value
        End Set
    End Property

    Private sWalkTo As String
    Public Property WalkTo As String
        Get
            Return sWalkTo
        End Get
        Set(value As String)
            If value <> sWalkTo Then
                sWalkTo = value
            End If
        End Set
    End Property


    Public Sub DoWalk()
        Static sLastPosition As String = ""

        If WalkTo <> "" Then
            If Location.LocationKey = sLastPosition Then
                WalkTo = "" ' Something has stopped us moving, so bomb out the walk
                Exit Sub
            End If
            Dim node As WalkNode = Dijkstra(Location.LocationKey, WalkTo)
            If node IsNot Nothing Then
                While node.iDistance > 1
                    node = node.Previous
                End While
                If Me Is Adventure.Player Then
                    For Each d As DirectionsEnum In [Enum].GetValues(GetType(DirectionsEnum))
                        If Adventure.htblLocations(Location.LocationKey).arlDirections(d).LocationKey = node.Key Then
                            If node.Key = WalkTo Then WalkTo = ""
                            fRunner.SetInput(d.ToString)
                            sLastPosition = Location.LocationKey
                            fRunner.SubmitCommand()
                            sLastPosition = ""
                            Exit Sub
                        End If
                    Next
                End If
            Else
                WalkTo = ""
            End If
        End If

    End Sub


    Private Class WalkNode
        Implements IComparable(Of WalkNode)

        Public Key As String
        Public iDistance As Integer = Integer.MaxValue
        Public Previous As WalkNode = Nothing

        Public Function CompareTo(other As WalkNode) As Integer Implements System.IComparable(Of WalkNode).CompareTo
            Return iDistance.CompareTo(other.iDistance)
        End Function
    End Class


    ' Adapted from http://en.wikipedia.org/wiki/Dijkstra's_algorithm
    '
    Private Function Dijkstra(ByVal sFrom As String, ByVal sTo As String) As WalkNode

        Dim WalkNodes As New Dictionary(Of String, WalkNode)
        Dim Q As New List(Of WalkNode)

        For Each sKey As String In Adventure.htblLocations.Keys
            Dim node As New WalkNode
            node.Key = sKey
            node.iDistance = Integer.MaxValue                   ' Unknown distance function from source to v
            node.Previous = Nothing                             ' Previous node in optimal path from source
            WalkNodes.Add(sKey, node)
            Q.Add(node)
        Next

        WalkNodes(sFrom).iDistance = 0                          ' Distance from source to source
        Q.Sort()

        While Q.Count > 0

            Dim u As WalkNode = Q(0)                            ' vertex in Q with smallest distance
            If u.iDistance = Integer.MaxValue Then Exit While ' all remaining vertices are inaccessible from source

            If u.Key = sTo Then Return u
            Q.Remove(u)

            For Each d As DirectionsEnum In [Enum].GetValues(GetType(DirectionsEnum))
                If HasRouteInDirection(d, False, u.Key) Then
                    Dim sDest As String = Adventure.htblLocations(u.Key).arlDirections(d).LocationKey
                    If Adventure.htblLocations(sDest).SeenBy(Me.Key) Then
                        Dim alt As Integer = u.iDistance + 1
                        If alt < WalkNodes(sDest).iDistance Then
                            WalkNodes(sDest).iDistance = alt
                            WalkNodes(sDest).Previous = u
                            Q.Sort()
                        End If
                    End If
                End If
            Next

        End While

        Return Nothing

    End Function


    Private ePerspective As PerspectiveEnum = PerspectiveEnum.SecondPerson ' Default for Player
    Friend Property Perspective As PerspectiveEnum
        Get
            If eCharacterType = CharacterTypeEnum.Player Then
                Return ePerspective
            Else
                Return PerspectiveEnum.ThirdPerson
            End If
        End Get
        Set(value As PerspectiveEnum)
            ePerspective = value
        End Set
    End Property


    ' This gets updated at the end of each turn, and allows any tasks to 
    ' reference the parent before they are updated from task actions
    '
    Private sPrevParent As String
    Public Property PrevParent() As String
        Get
            Return sPrevParent
        End Get
        Set(ByVal value As String)
            sPrevParent = value
        End Set
    End Property


    Friend Overrides ReadOnly Property Parent() As String
        Get
            Return Location.Key
        End Get
    End Property


    Public Enum CharacterTypeEnum
        Player
        NonPlayer
    End Enum

    Public Enum GenderEnum
        Male = 0
        Female = 1
        Unknown = 2
    End Enum

    Public Property Gender() As GenderEnum
        Get
            Select Case GetPropertyValue("Gender")
                Case "Male"
                    Return GenderEnum.Male
                Case "Female"
                    Return GenderEnum.Female
                Case Else
                    Return GenderEnum.Unknown
            End Select
        End Get
        Set(ByVal Value As GenderEnum)
            SetPropertyValue("Gender", Value.ToString)
        End Set
    End Property


    Public Property CharacterType() As CharacterTypeEnum
        Get
            Return eCharacterType
        End Get
        Set(ByVal Value As CharacterTypeEnum)
            eCharacterType = Value
        End Set
    End Property


    Public Property HasSeenLocation(ByVal sLocKey As String) As Boolean
        Get
            If htblSeenLocation.ContainsKey(sLocKey) Then
                Return CBool(htblSeenLocation(sLocKey))
            Else
                Return False
            End If
        End Get
        Set(ByVal Value As Boolean)
            If Not htblSeenLocation.ContainsKey(sLocKey) Then
                htblSeenLocation.Add(Value, sLocKey)
            Else
                htblSeenLocation(sLocKey) = Value
            End If
            If Me Is Adventure.Player Then
                For Each p As MapPage In Adventure.Map.Pages.Values
                    For Each n As MapNode In p.Nodes
                        If n.Key = sLocKey Then
                            n.Seen = Value
                            Exit Property
                        End If
                    Next
                Next
            End If
        End Set
    End Property

    Public Property HasSeenObject(ByVal sObKey As String) As Boolean
        Get
            If htblSeenObject.ContainsKey(sObKey) Then
                Return CBool(htblSeenObject(sObKey))
            Else
                Return False
            End If
        End Get
        Set(ByVal Value As Boolean)
            If Not htblSeenObject.ContainsKey(sObKey) Then
                htblSeenObject.Add(Value, sObKey)
            Else
                htblSeenObject(sObKey) = Value
            End If
        End Set
    End Property

    Public Property HasSeenCharacter(ByVal sCharKey As String) As Boolean
        Get
            If htblSeenChar.ContainsKey(sCharKey) Then
                Return CBool(htblSeenChar(sCharKey))
            Else
                Return False
            End If
        End Get
        Set(ByVal Value As Boolean)
            If Not htblSeenChar.ContainsKey(sCharKey) Then
                htblSeenChar.Add(Value, sCharKey)
            Else
                htblSeenChar(sCharKey) = Value
            End If
        End Set
    End Property

    Public Property SeenBy(ByVal sCharKey As String) As Boolean
        Get
            Return Adventure.htblCharacters(sCharKey).HasSeenCharacter(Key)
        End Get
        Set(ByVal Value As Boolean)
            Adventure.htblCharacters(sCharKey).HasSeenCharacter(Key) = Value
        End Set
    End Property


    Public ReadOnly Property AloneWithChar As String
        Get
            Dim iCount As Integer = 0
            Dim sChar As String = Nothing
            For Each ch As clsCharacter In Adventure.htblCharacters.Values
                If ch IsNot Me Then
                    If Location.LocationKey = ch.Location.LocationKey Then
                        iCount += 1
                        sChar = ch.Key
                    End If
                End If
                If iCount > 1 Then Return Nothing
            Next
            If iCount = 1 Then Return sChar Else Return Nothing
        End Get
    End Property

    ' ExplicitPronoun - The user has said they want it to be this pronoun, so don't auto-switch it
    Friend ReadOnly Property Name(Optional ByVal Pronoun As PronounEnum = PronounEnum.Subjective, Optional bMarkAsSeen As Boolean = True, Optional ByVal bAllowPronouns As Boolean = True, Optional ByVal Article As ArticleTypeEnum = ArticleTypeEnum.Indefinite, Optional ByVal bForcePronoun As Boolean = False, Optional ByVal bExplicitArticle As Boolean = False) As String
        Get
            With UserSession


                Dim bReplaceWithPronoun As Boolean = bForcePronoun

                If .bDisplaying Then
                    If Not bExplicitArticle AndAlso Introduced AndAlso Article = ArticleTypeEnum.Indefinite Then Article = ArticleTypeEnum.Definite
                    If bMarkAsSeen Then
                        Introduced = True
                        If Not Adventure.lCharMentionedThisTurn(Gender).Contains(Key) Then Adventure.lCharMentionedThisTurn(Gender).Add(Key)
                    End If

                    ' If we already have mentioned this key, and it's the most recent key, we can replace with pronoun
                    If Perspective = PerspectiveEnum.FirstPerson OrElse Perspective = PerspectiveEnum.SecondPerson Then bReplaceWithPronoun = True

                    If .PronounKeys.Count > 0 Then
                        Dim oPreviousPronoun As PronounInfo = Nothing
                        For i As Integer = .PronounKeys.Count - 1 To 0 Step -1
                            With .PronounKeys(i)
                                If .Gender = Gender AndAlso .Key = Key Then
                                    oPreviousPronoun = UserSession.PronounKeys(i)
                                    Exit For
                                End If
                            End With
                        Next
                        If oPreviousPronoun IsNot Nothing Then
                            bReplaceWithPronoun = True
                            If oPreviousPronoun.Pronoun = PronounEnum.Subjective AndAlso Pronoun = PronounEnum.Objective Then
                                Pronoun = PronounEnum.Reflective
                            End If
                        End If
                    End If
                End If
                If Pronoun = PronounEnum.None Then bReplaceWithPronoun = False

                If Not bAllowPronouns OrElse Not bReplaceWithPronoun Then
                    ' Display the actual name/descriptor
                    Dim sName As String
                    If HasProperty("Known") Then
                        If GetProperty("Known").Selected Then
                            sName = ProperName
                        Else
                            sName = Descriptor(Article)
                        End If
                    Else
                        If Descriptor <> "" Then
                            sName = Descriptor(Article)
                        Else
                            sName = ProperName
                        End If
                    End If
                    If Pronoun = PronounEnum.Possessive Then
                        Return sName & "'s"
                    Else
                        Return sName
                    End If
                Else
                    ' Display the pronoun
                    Select Case Perspective
                        Case PerspectiveEnum.FirstPerson
                            Select Case Pronoun
                                Case PronounEnum.Objective
                                    Return "me"
                                Case PronounEnum.Possessive
                                    Return "my" ' "mine"
                                Case PronounEnum.Reflective
                                    Return "myself"
                                Case PronounEnum.Subjective
                                    Return "I"
                            End Select
                        Case PerspectiveEnum.SecondPerson
                            Select Case Pronoun
                                Case PronounEnum.Objective
                                    Return "you"
                                Case PronounEnum.Possessive
                                    Return "your" ' "yours"
                                Case PronounEnum.Reflective
                                    Return "yourself"
                                Case PronounEnum.Subjective
                                    Return "you"
                            End Select
                        Case PerspectiveEnum.ThirdPerson
                            Select Case Pronoun
                                Case PronounEnum.Objective
                                    Select Case Gender
                                        Case GenderEnum.Male
                                            Return "him"
                                        Case GenderEnum.Female
                                            Return "her"
                                        Case GenderEnum.Unknown
                                            Return "it"
                                    End Select
                                Case PronounEnum.Possessive
                                    Select Case Gender
                                        Case GenderEnum.Male
                                            Return "his"
                                        Case GenderEnum.Female
                                            Return "her" ' "hers"
                                        Case GenderEnum.Unknown
                                            Return "its"
                                    End Select
                                Case PronounEnum.Reflective
                                    Select Case Gender
                                        Case GenderEnum.Male
                                            Return "himself"
                                        Case GenderEnum.Female
                                            Return "herself"
                                        Case GenderEnum.Unknown
                                            Return "itself"
                                    End Select
                                Case PronounEnum.Subjective
                                    Select Case Gender
                                        Case GenderEnum.Male
                                            Return "he"
                                        Case GenderEnum.Female
                                            Return "she"
                                        Case GenderEnum.Unknown
                                            Return "it"
                                    End Select
                            End Select
                    End Select
                End If

                Return ""

            End With
        End Get
    End Property


    Friend Property ProperName(Optional ByVal Pronoun As PronounEnum = PronounEnum.Subjective) As String
        Get
            If sName <> "" Then Return sName Else Return "Anonymous"
        End Get
        Set(ByVal Value As String)
            sName = Value
        End Set
    End Property


    Friend ReadOnly Property Descriptor(Optional ByVal Article As ArticleTypeEnum = ArticleTypeEnum.Indefinite) As String
        Get
            If arlDescriptors.Count > 0 Then
                Dim sArticle2 As String = Nothing ' sArticle
                Select Case Article
                    Case ArticleTypeEnum.Definite
                        If sArticle <> "" Then sArticle2 = "the"
                    Case ArticleTypeEnum.Indefinite
                        sArticle2 = sArticle
                    Case ArticleTypeEnum.None
                        sArticle2 = ""
                End Select
                'If TheName AndAlso sArticle <> "" Then sArticle2 = "the"
                If sArticle <> "" Then sArticle2 &= " "
                If sPrefix <> "" Then
                    Return sArticle2 & sPrefix & " " & arlDescriptors(0)
                Else
                    Return sArticle2 & arlDescriptors(0)
                End If
            Else
                Return ""
            End If
        End Get
    End Property


    Public Sub Move(ByVal ToWhere As String)

        If Adventure.htblLocations.ContainsKey(ToWhere) Then
            Dim loc As New clsCharacterLocation(Me)
            loc.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation
            loc.Key = ToWhere
            Move(loc)
        ElseIf ToWhere = HIDDEN Then
            Dim loc As New clsCharacterLocation(Me)
            loc.ExistWhere = clsCharacterLocation.ExistsWhereEnum.Hidden
            loc.Key = ""
            Move(loc)
        End If

    End Sub
    Public Sub Move(ByVal ToWhere As clsCharacterLocation)
        If Me Is Adventure.Player Then
            For Each t As clsTask In Adventure.Tasks(clsAdventure.TasksListEnum.SystemTasks).Values
                If t.Repeatable OrElse Not t.Completed Then
                    If Adventure.Player.Location.LocationKey <> ToWhere.LocationKey AndAlso t.LocationTrigger = ToWhere.LocationKey Then
                        ' Ok, we need to trigger this task                        
                        Adventure.qTasksToRun.Enqueue(t.Key)
                    End If
                End If
            Next
        End If

        ' Ensure that character Key is not this character, or on this character
        Me.Location = ToWhere

        ' Update any 'seen' things        
        If ToWhere.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation Then
            Location.sLastLocationKey = ToWhere.Key
            If htblSeenLocation.ContainsKey(ToWhere.Key) Then htblSeenLocation(ToWhere.Key) = True Else htblSeenLocation.Add(True, ToWhere.Key)
            If Adventure.htblLocations.ContainsKey(ToWhere.Key) Then
                For Each ob As clsObject In Adventure.htblLocations(ToWhere.Key).ObjectsInLocation(clsLocation.WhichObjectsToListEnum.AllObjects, True).Values
                    If htblSeenObject.ContainsKey(ob.Key) Then htblSeenObject(ob.Key) = True Else htblSeenObject.Add(True, ob.Key)
                Next
                For Each ch As clsCharacter In Adventure.htblLocations(ToWhere.Key).CharactersVisibleAtLocation.Values
                    If htblSeenChar.ContainsKey(ch.Key) Then htblSeenChar(ch.Key) = True Else htblSeenChar.Add(True, ch.Key)
                Next
            End If
        End If

        If Adventure.sConversationCharKey <> "" Then
            If Adventure.htblCharacters(Adventure.sConversationCharKey).Location.Key <> Adventure.Player.Location.Key Then
                If Me.Key = Adventure.Player.Key Then
                    Dim farewell As clsTopic = UserSession.FindConversationNode(Adventure.htblCharacters(Adventure.sConversationCharKey), clsAction.ConversationEnum.Farewell, "")
                    If farewell IsNot Nothing Then
                        UserSession.Display(farewell.oConversation.ToString)
                    End If
                End If
                Adventure.sConversationCharKey = ""
                Adventure.sConversationNode = ""
            End If
        End If

        If Me Is Adventure.Player Then
            Adventure.Map.RefreshNode(Adventure.Player.Location.LocationKey)
            UserSession.Map.SelectNode(Adventure.Player.Location.LocationKey)
        End If
    End Sub


    Public Property Location() As clsCharacterLocation
        Get
            Return cLocation
        End Get
        Set(ByVal Value As clsCharacterLocation)
            Try
                cLocation = Value
            Catch ex As Exception
                ErrMsg("Error moving character " & Name & " to location", ex)
            End Try
        End Set

    End Property


    Public Property MaxSize() As Integer
        Get
            Return iMaxSize
        End Get
        Set(ByVal Value As Integer)
            iMaxSize = Value
        End Set
    End Property


    Public Property MaxWeight() As Integer
        Get
            Return iMaxWeight
        End Get
        Set(ByVal Value As Integer)
            iMaxWeight = Value
        End Set
    End Property

    Public Property Article() As String
        Get
            Return sArticle
        End Get
        Set(ByVal Value As String)
            sArticle = Value
        End Set
    End Property

    Public Property Prefix() As String
        Get
            Return sPrefix
        End Get
        Set(ByVal Value As String)
            sPrefix = Value
        End Set
    End Property

    Friend Property Description() As Description
        Get
            If oDescription Is Nothing Then oDescription = New Description
            Return oDescription
        End Get
        Set(ByVal Value As Description)
            oDescription = Value
        End Set
    End Property

    Public Property IsHereDesc() As String
        Get
            Return GetPropertyValue("CharHereDesc")
        End Get
        Set(ByVal Value As String)
            If Value <> "" Then
                SetPropertyValue("CharHereDesc", Value)
            Else
                RemoveProperty("CharHereDesc")
            End If
        End Set
    End Property

    Public Overrides ReadOnly Property Clone() As clsItem
        Get
            Dim ch As clsCharacter = CType(Me.MemberwiseClone, clsCharacter)
            ch.htblActualProperties = CType(ch.htblActualProperties.Clone, PropertyHashTable)
            ch.arlDescriptors = ch.arlDescriptors.Clone
            Return ch
        End Get
    End Property


    Friend ReadOnly Property ChildrenObjects(ByVal WhereChildren As clsObject.WhereChildrenEnum, Optional ByVal bRecursive As Boolean = False) As ObjectHashTable
        Get
            Dim htblChildren As New ObjectHashTable

            For Each ob As clsObject In Adventure.htblObjects.Values
                If Not ob.IsStatic Then
                    If ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter Or ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter Then
                        If ob.Location.Key = Me.Key Then
                            htblChildren.Add(ob, ob.Key)
                            If bRecursive Then
                                For Each obChild As clsObject In ob.Children(clsObject.WhereChildrenEnum.Everything, True).Values
                                    htblChildren.Add(obChild, obChild.Key)
                                Next
                            End If
                        End If
                    End If
                End If
            Next

            Return htblChildren
        End Get
    End Property

    Friend dictHasRouteCache As New Generic.Dictionary(Of String, Boolean)
    Friend dictRouteErrors As New Generic.Dictionary(Of String, String)
    Public Function HasRouteInDirection(ByVal drn As DirectionsEnum, ByVal bIgnoreRestrictions As Boolean, Optional ByVal sFromLocation As String = "", Optional ByRef sErrorMessage As String = "") As Boolean

        If sFromLocation = "" Then sFromLocation = Location.LocationKey
        If Not Adventure.htblLocations.ContainsKey(sFromLocation) Then Return False

        Dim d As clsDirection = Adventure.htblLocations(sFromLocation).arlDirections(drn)
        If d.LocationKey <> "" Then
            If bIgnoreRestrictions Then
                Return True
            Else
                Dim bResult As Boolean
                If dictHasRouteCache.ContainsKey(sFromLocation & drn.ToString) Then
                    bResult = dictHasRouteCache(sFromLocation & drn.ToString)
                    If Not bResult Then sErrorMessage = dictRouteErrors(sFromLocation & drn.ToString)
                    Return bResult
                End If
                ' evaluate direction restrictions                    
                bResult = UserSession.PassRestrictions(d.Restrictions)
                If Not bResult Then sErrorMessage = UserSession.sRestrictionText
                dictHasRouteCache.Add(sFromLocation & drn.ToString, bResult)
                dictRouteErrors.Add(sFromLocation & drn.ToString, sErrorMessage)
                If Not bResult Then d.bEverBeenBlocked = True
                Return bResult
            End If
        Else
            Return False
        End If

    End Function

    Public ReadOnly Property IsInGroupOrLocation(ByVal sGroupOrLocationKey As String) As Boolean
        Get
            If Adventure.htblLocations.ContainsKey(sGroupOrLocationKey) Then
                If Me.Location.LocationKey = sGroupOrLocationKey Then
                    Return True
                End If
            ElseIf Adventure.htblGroups.ContainsKey(sGroupOrLocationKey) Then
                If Adventure.htblGroups(sGroupOrLocationKey).arlMembers.Contains(Me.Location.LocationKey) Then
                    Return True
                End If
            End If

            Return False
        End Get
    End Property

    Public ReadOnly Property BoundVisible As String
        Get
            With Me.Location
                Select Case .ExistWhere
                    Case clsCharacterLocation.ExistsWhereEnum.AtLocation
                        Return .Key
                    Case clsCharacterLocation.ExistsWhereEnum.Hidden
                        Return HIDDEN
                    Case clsCharacterLocation.ExistsWhereEnum.InObject
                        With Adventure.htblObjects(.Key)
                            If Not .Openable OrElse .IsOpen OrElse .IsTransparent Then
                                Return Adventure.htblObjects(.Key).BoundVisible
                            Else
                                Return .Key
                            End If
                        End With
                    Case clsCharacterLocation.ExistsWhereEnum.OnCharacter
                        Return Adventure.htblCharacters(.Key).BoundVisible
                    Case clsCharacterLocation.ExistsWhereEnum.OnObject
                        Return Adventure.htblObjects(.Key).BoundVisible
                    Case clsCharacterLocation.ExistsWhereEnum.Uninitialised
                        Return HIDDEN
                End Select
                Return HIDDEN
            End With
        End Get
    End Property


    Friend Function CanSeeCharacter(ByVal sCharKey As String) As Boolean
        Return IsVisibleTo(sCharKey)
    End Function
    Public ReadOnly Property IsVisibleTo(ByVal sCharKey As String) As Boolean
        Get
            If sCharKey = "%Player%" Then sCharKey = Adventure.Player.Key

            Dim sBoundVisible As String = BoundVisible
            Dim sBoundVisibleCh As String = Adventure.htblCharacters(sCharKey).BoundVisible

            If sBoundVisible = HIDDEN Then Return False

            Return sBoundVisible = sBoundVisibleCh

        End Get
    End Property


    Public ReadOnly Property IsVisibleAtLocation(ByVal sLocationKey As String) As Boolean
        Get
            Dim sBoundVisible As String = BoundVisible

            If sBoundVisible = HIDDEN Then Return False

            If Adventure.htblGroups.ContainsKey(sBoundVisible) Then
                Return Adventure.htblGroups(sBoundVisible).arlMembers.Contains(sLocationKey)
            Else
                Return sBoundVisible = sLocationKey
            End If
        End Get
    End Property

    Public ReadOnly Property CanSeeObject(ByVal sObKey As String) As Boolean
        Get
            Dim sBoundVisible As String = BoundVisible

            If sBoundVisible = HIDDEN Then Return False

            If Adventure.htblGroups.ContainsKey(sObKey) Then
                For Each sOb As String In Adventure.htblGroups(sObKey).arlMembers
                    If CanSeeObject(sOb) Then Return True
                Next
                Return False
            End If

            Dim sBoundVisibleOb As String = Adventure.htblObjects(sObKey).BoundVisible

            If Adventure.htblGroups.ContainsKey(sBoundVisibleOb) Then
                Return Adventure.htblGroups(sBoundVisibleOb).arlMembers.Contains(sBoundVisible)
            Else
                Return sBoundVisible = sBoundVisibleOb OrElse sBoundVisible = sObKey ' Allow us to see the object we're in, otherwise we can't do anything with it (open it again!)
            End If
        End Get
    End Property


    Public ReadOnly Property IsWearingObject(ByVal sObKey As String, Optional ByVal bDirectly As Boolean = True) As Boolean
        Get
            If sObKey = NOOBJECT Then Return WornObjects.Count = 0
            If sObKey = ANYOBJECT Then Return WornObjects.Count > 0

            With Adventure.htblObjects(sObKey).Location
                Select Case .DynamicExistWhere
                    Case clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                        If .Key = Me.Key OrElse (.Key = "%Player%" AndAlso Me.eCharacterType = CharacterTypeEnum.Player) Then Return True
                    Case clsObjectLocation.DynamicExistsWhereEnum.InObject, clsObjectLocation.DynamicExistsWhereEnum.OnObject
                        If Not bDirectly Then
                            Return IsWearingObject(Adventure.htblObjects(sObKey).Parent)
                        Else
                            Return False
                        End If
                End Select
                Return False
            End With
        End Get
    End Property


    Public ReadOnly Property sRegularExpressionString(Optional ByVal bMyRE As Boolean = False, Optional ByVal bPlural As Boolean = False, Optional ByVal bPrefixMandatory As Boolean = False) As String
        Get
            Dim arl As StringArrayList = arlDescriptors

            If bMyRE Then
                ' The ADRIFT 'advanced command construction' expression
                Dim sRE As String = "{" & sArticle & "/the} "
                If sPrefix <> "" Then
                    For Each sSinglePrefix As String In Split(sPrefix, " ")
                        If sSinglePrefix <> "" Then sRE &= "{" & sSinglePrefix.ToLower & "} "
                    Next
                End If
                sRE &= "["
                For Each sNoun As String In arl
                    sRE &= sNoun.ToLower & "/"
                Next
                If Not Adventure.htblCharacterProperties.ContainsKey("Known") OrElse HasProperty("Known") OrElse arl.Count = 0 Then sRE &= ProperName.ToLower & "/"
                Return Left(sRE, sRE.Length - 1) & "]"
            Else
                ' Real Regular Expressions
                Dim sRE As String = "("
                If sArticle <> "" Then sRE &= sArticle & " |"
                sRE &= "the )?"
                For Each sSinglePrefix As String In Split(sPrefix, " ")
                    If sSinglePrefix <> "" Then sRE &= "(" & sSinglePrefix.ToLower & " )?"
                Next
                sRE &= "("
                For Each sNoun As String In arl
                    sRE &= sNoun.ToLower & "|"
                Next
                If Not Adventure.htblCharacterProperties.ContainsKey("Known") OrElse HasProperty("Known") OrElse arl.Count = 0 Then sRE &= ProperName.ToLower & "|"
                Return Left(sRE, sRE.Length - 1) & ")"
            End If

        End Get
    End Property


    ' Returns True if character is directly inside parent, or if on/in something that is inside it
    Public ReadOnly Property IsInside(ByVal sParentKey As String) As Boolean
        Get
            If (Me.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject OrElse Me.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.InObject) AndAlso Parent <> "" Then
                If Me.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.InObject AndAlso Parent = sParentKey Then
                    Return True
                Else
                    Dim obParent As clsObject = Adventure.htblObjects(Parent)
                    Return obParent.IsInside(sParentKey)
                End If
            Else
                Return False
            End If
        End Get
    End Property


    ' Returns True if character is directly on parent, or if on/in something that is inside it
    Public ReadOnly Property IsOn(ByVal sParentKey As String) As Boolean
        Get
            If (Me.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject OrElse Me.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.InObject OrElse Me.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnCharacter) AndAlso Parent <> "" Then
                If (Me.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject OrElse Me.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnCharacter) AndAlso Parent = sParentKey Then
                    Return True
                Else
                    If Adventure.htblObjects.ContainsKey(Parent) Then
                        Dim obParent As clsObject = Adventure.htblObjects(Parent)
                        Return obParent.IsOn(sParentKey)
                    ElseIf Adventure.htblCharacters.ContainsKey(Parent) Then
                        Dim chParent As clsCharacter = Adventure.htblCharacters(Parent)
                        Return chParent.IsOn(sParentKey)
                    End If
                End If
            Else
                Return False
            End If
        End Get
    End Property


    Public ReadOnly Property IsHoldingObject(ByVal sObKey As String, Optional ByVal bDirectly As Boolean = False) As Boolean
        Get
            If sObKey = NOOBJECT Then Return HeldObjects.Count = 0
            If sObKey = ANYOBJECT Then Return HeldObjects.Count > 0

            With Adventure.htblObjects(sObKey).Location
                Select Case .DynamicExistWhere
                    Case clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter
                        If .Key = Me.Key OrElse (.Key = "%Player%" AndAlso Me.eCharacterType = CharacterTypeEnum.Player) Then Return True
                    Case clsObjectLocation.DynamicExistsWhereEnum.InObject, clsObjectLocation.DynamicExistsWhereEnum.OnObject
                        If Not bDirectly Then
                            Return IsHoldingObject(Adventure.htblObjects(sObKey).Location.Key)
                        Else
                            Return False
                        End If
                End Select
                Return False
            End With
        End Get
    End Property


    Public ReadOnly Property IsAlone() As Boolean
        Get
            For Each chr As clsCharacter In Adventure.htblCharacters.Values
                If chr.Key <> Key AndAlso chr.Location.LocationKey = Location.LocationKey Then Return False
            Next
            Return True
        End Get
    End Property



    Friend ReadOnly Property Children(Optional ByVal bRecursive As Boolean = False) As CharacterHashTable
        Get
            Dim htblChildren As New CharacterHashTable

            For Each ch As clsCharacter In Adventure.htblCharacters.Values
                If ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnCharacter Then
                    If ch.Location.Key = Me.Key Then
                        htblChildren.Add(ch, ch.Key)
                        If bRecursive Then
                            For Each chChild As clsCharacter In ch.Children(True).Values
                                htblChildren.Add(chChild, chChild.Key)
                            Next
                        End If
                    End If
                End If
            Next
            If bRecursive Then
                For Each ob As clsObject In Adventure.htblObjects.Values
                    If ob.IsStatic Then
                        Select Case ob.Location.StaticExistWhere
                            Case clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter
                                If ob.Location.Key = Me.Key Then
                                    For Each chInOrOnObThatIsPartOfChar As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideOrOnObject).Values
                                        htblChildren.Add(chInOrOnObThatIsPartOfChar, chInOrOnObThatIsPartOfChar.Key)
                                    Next
                                End If
                        End Select
                    Else
                        Select Case ob.Location.DynamicExistWhere
                            Case clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter, clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                                If ob.Location.Key = Me.Key Then
                                    For Each chInOrOnObThatIsHeldWornByChar As clsCharacter In ob.ChildrenCharacters(clsObject.WhereChildrenEnum.InsideOrOnObject).Values
                                        htblChildren.Add(chInOrOnObThatIsHeldWornByChar, chInOrOnObThatIsHeldWornByChar.Key)
                                    Next
                                End If
                        End Select
                    End If
                Next
            End If

            Return htblChildren
        End Get
    End Property

    Friend ReadOnly Property HeldObjects(Optional ByVal bRecursive As Boolean = False) As ObjectHashTable
        Get
            Dim htblHeldObjects As New ObjectHashTable

            For Each ob As clsObject In Adventure.DynamicObjects ' Adventure.htblObjects.Values
                If ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter Then
                    If ob.Location.Key = Me.Key OrElse (ob.Location.Key = "%Player%" AndAlso Me.eCharacterType = CharacterTypeEnum.Player) Then
                        htblHeldObjects.Add(ob, ob.Key)
                        If bRecursive Then
                            For Each obChild As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideOrOnObject, True).Values
                                htblHeldObjects.Add(obChild, obChild.Key)
                            Next
                        End If
                    End If
                End If
            Next

            Return htblHeldObjects
        End Get
    End Property

    Friend ReadOnly Property WornObjects(Optional ByVal bRecursive As Boolean = False) As ObjectHashTable
        Get
            Dim htblWornObjects As New ObjectHashTable

            For Each ob As clsObject In Adventure.htblObjects.Values
                If ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter Then
                    If ob.Location.Key = Me.Key OrElse (ob.Location.Key = "%Player%" AndAlso Me.eCharacterType = CharacterTypeEnum.Player) Then
                        htblWornObjects.Add(ob, ob.Key)
                        If bRecursive Then
                            For Each obChild As clsObject In ob.Children(clsObject.WhereChildrenEnum.InsideOrOnObject, True).Values
                                htblWornObjects.Add(obChild, obChild.Key)
                            Next
                        End If
                    End If
                End If
            Next

            Return htblWornObjects
        End Get
    End Property

    Public Function ListExits(Optional ByVal sFromLocation As String = "", Optional ByRef iExitCount As Integer = 0) As String
        Dim sExits As String = ""

        If sFromLocation = "" Then sFromLocation = Adventure.Player.Location.LocationKey
        For d As DirectionsEnum = DirectionsEnum.North To DirectionsEnum.NorthWest ' Adventure.iCompassPoints
            If Me.HasRouteInDirection(d, False, sFromLocation) Then
                sExits &= DirectionName(d) & ", "
                iExitCount += 1
            End If
        Next

        If Right(sExits, 2) = ", " Then sExits = Left(sExits, sExits.Length - 2) & ""

        If iExitCount > 1 Then
            sExits = Left(sExits, sExits.LastIndexOf(", ")) & " and " & Right(sExits, sExits.Length - sExits.LastIndexOf(", ") - 2)
        End If

        If sExits = "" Then sExits = "nowhere"

        Return LCase(sExits)
    End Function

    Friend Overrides ReadOnly Property AllDescriptions() As Generic.List(Of SharedModule.Description)
        Get
            Dim all As New Generic.List(Of Description)
            all.Add(oDescription)
            For Each p As clsProperty In htblProperties.Values
                all.Add(p.StringData)
            Next
            For Each t As clsTopic In htblTopics.Values
                For Each r As clsRestriction In t.arlRestrictions
                    all.Add(r.oMessage)
                Next
                all.Add(t.oConversation)
            Next
            For Each w As clsWalk In arlWalks
                For i As Integer = 0 To w.SubWalks.Length - 1
                    all.Add(w.SubWalks(i).oDescription)
                Next
            Next
            Return all
        End Get
    End Property

    Protected Overrides ReadOnly Property PropertyGroupType() As clsGroup.GroupTypeEnum
        Get
            Return clsGroup.GroupTypeEnum.Characters
        End Get
    End Property


    Public Overrides Function ReferencesKey(ByVal sKey As String) As Integer

        Dim iCount As Integer = 0
        For Each d As Description In AllDescriptions
            iCount += d.ReferencesKey(sKey)
        Next
        For Each w As clsWalk In arlWalks
            For Each s As clsWalk.clsStep In w.arlSteps
                If s.sLocation = sKey Then iCount += 1
            Next
            For i As Integer = 0 To w.SubWalks.Length - 1
                With w.SubWalks(i)
                    If .sKey2 = sKey OrElse .sKey3 = sKey Then iCount += 1
                End With
            Next
            For i As Integer = 0 To w.WalkControls.Length - 1
                With w.WalkControls(i)
                    If .sTaskKey = sKey Then iCount += 1
                End With
            Next
        Next
        For Each t As clsTopic In htblTopics.Values
            iCount += t.arlRestrictions.ReferencesKey(sKey)
            iCount += t.arlActions.ReferencesKey(sKey)
        Next
        If Location.Key = sKey Then iCount += 1
        iCount += htblActualProperties.ReferencesKey(sKey)

        Return iCount

    End Function


    Public Overrides Function DeleteKey(ByVal sKey As String) As Boolean

        For Each d As Description In AllDescriptions
            If Not d.DeleteKey(sKey) Then Return False
        Next
        For Each w As clsWalk In arlWalks
            For i As Integer = w.arlSteps.Count - 1 To 0 Step -1
                If CType(w.arlSteps(i), clsWalk.clsStep).sLocation = sKey Then w.arlSteps.RemoveAt(i)
            Next
            For i As Integer = 0 To w.SubWalks.Length - 1
                If w.SubWalks(i).sKey2 = sKey Then w.SubWalks(i).sKey2 = ""
                If w.SubWalks(i).sKey3 = sKey Then w.SubWalks(i).sKey3 = ""
            Next
            For i As Integer = 0 To w.WalkControls.Length - 1
                If w.WalkControls(i).sTaskKey = sKey Then
                    For j As Integer = w.WalkControls.Length - 2 To i Step -1
                        w.WalkControls(j) = w.WalkControls(j + 1)
                    Next
                    ReDim Preserve w.WalkControls(w.WalkControls.Length - 2)
                End If
            Next
        Next
        For Each t As clsTopic In htblTopics.Values
            If Not t.arlRestrictions.DeleteKey(sKey) Then Return False
            If Not t.arlActions.DeleteKey(sKey) Then Return False
        Next
        If Location.Key = sKey Then
            Location.Key = ""
            Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.Hidden
        End If
        If Not htblActualProperties.DeleteKey(sKey) Then Return False

        Return True

    End Function


    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object
        Dim iCount As Integer = iReplacements
        iReplacements += MyBase.FindStringInStringProperty(Me.sName, sSearchString, sReplace, bFindAll)
        iReplacements += MyBase.FindStringInStringProperty(Me.sArticle, sSearchString, sReplace, bFindAll)
        For i As Integer = arlDescriptors.Count - 1 To 0 Step -1
            iReplacements += MyBase.FindStringInStringProperty(Me.arlDescriptors(i), sSearchString, sReplace, bFindAll)
        Next
        Return iReplacements - iCount
    End Function

End Class



<DebuggerDisplay("{Summary} : {Keywords}")>
Public Class clsTopic

    Public Key As String
    Public ParentKey As String
    Public Summary As String
    Public Keywords As String
    Public bIntroduction, bAsk, bTell, bCommand, bFarewell, bStayInNode As Boolean
    Friend oConversation As Description
    Friend arlRestrictions As New RestrictionArrayList
    Friend arlActions As New ActionArrayList

    Public Sub New()
        oConversation = New Description
    End Sub

    Public Function Clone() As clsTopic
        Dim topic As clsTopic = CType(Me.MemberwiseClone, clsTopic)
        topic.arlRestrictions = topic.arlRestrictions.Copy
        topic.arlActions = topic.arlActions.Copy
        Return topic
    End Function

End Class


Public Class clsWalk

    Public Class clsStep
        Public sLocation As String
        Friend ftTurns As New FromTo
    End Class

    Private sDescription As String
    Private bLoop As Boolean
    Private sFromDesc As String
    Private sToDesc As String
    Private bShowMove As Boolean
    Private bStartActive As Boolean
    Friend iTimerToEndOfWalk As Integer
    Private iLastSubWalkTime As Integer = 0

    Public sKey As String ' This is the key of the character

    Private Enum Command
        [Nothing] = 0
        Start = 1
        [Stop] = 2
        Pause = 3
        [Resume] = 4
        Restart = 5
    End Enum
    Private NextCommand As Command = Command.Nothing

    Public Enum StatusEnum
        NotYetStarted = 0
        Running = 1
        Paused = 3
        Finished = 4
    End Enum
    Friend Status As StatusEnum


    Public Class SubWalk
        Implements ICloneable

        Public Enum WhenEnum
            FromLastSubWalk
            FromStartOfWalk
            BeforeEndOfWalk
            ComesAcross
        End Enum
        Public eWhen As WhenEnum

        Public Enum WhatEnum
            DisplayMessage
            ExecuteTask
            UnsetTask
        End Enum
        Public eWhat As WhatEnum

        Friend ftTurns As New FromTo
        Friend oDescription As New Description
        Public sKey As String
        Public sKey2 As String
        Public sKey3 As String

        Friend bSameLocationAsChar As Boolean = False

        Private Function Clone() As Object Implements System.ICloneable.Clone
            Dim se As SubWalk
            se = CType(Me.MemberwiseClone, SubWalk)
            se.ftTurns = ftTurns.CloneMe
            se.oDescription = oDescription.Copy
            Return se
        End Function
        Public Function CloneMe() As SubWalk
            Return CType(Me.Clone, SubWalk)
        End Function

    End Class
    Public SubWalks() As SubWalk = {}

    Public arlSteps As New ArrayList

    Friend WalkControls() As EventOrWalkControl = {}

    Public Property Description() As String
        Get
            Return sDescription
        End Get
        Set(ByVal value As String)
            sDescription = value
        End Set
    End Property

    Public Property Loops() As Boolean
        Get
            Return bLoop
        End Get
        Set(ByVal Value As Boolean)
            bLoop = Value
        End Set
    End Property

    Public Property StartActive() As Boolean
        Get
            Return bStartActive
        End Get
        Set(ByVal value As Boolean)
            bStartActive = value
        End Set
    End Property


    Public Function GetDefaultDescription() As String
        Dim sb As New System.Text.StringBuilder

        For Each stp As clsStep In arlSteps
            Select Case stp.sLocation
                Case "Hidden"
                    sb.Append("Hidden")
                Case Else
                    Dim sDescription As String = Adventure.GetNameFromKey(stp.sLocation)
                    If Adventure.htblCharacters.ContainsKey(stp.sLocation) Then sDescription = "Follow " & sDescription
                    If sDescription <> "" Then
                        sb.Append(sDescription)
                    Else
                        sb.Append("<" & stp.sLocation & ">") ' Groups may not be defined yet
                    End If
            End Select
            If stp.ftTurns.iFrom = stp.ftTurns.iTo Then
                sb.Append(" [" & stp.ftTurns.iFrom & "]")
            Else
                sb.Append(" [" & stp.ftTurns.iFrom & " - " & stp.ftTurns.iTo & "]")
            End If

            If stp IsNot arlSteps(arlSteps.Count - 1) Then sb.Append(" -> ")
        Next
        If bLoop Then sb.Append(" {L}")

        If sb.ToString = "" Then sb.Append("Unnamed walk")

        Return sb.ToString

    End Function

    Private Sub ResetLength()
        For Each [step] As clsWalk.clsStep In Me.arlSteps
            [step].ftTurns.Reset()
        Next
    End Sub
    Private ReadOnly Property Length() As Integer
        Get
            Dim iLength As Integer = 0
            For Each [step] As clsWalk.clsStep In Me.arlSteps
                iLength += [step].ftTurns.Value
            Next
            Return iLength
        End Get
    End Property


    Public Property TimerToEndOfWalk() As Integer
        Get
            Return iTimerToEndOfWalk
        End Get
        Set(ByVal value As Integer)
            Dim iStartValue As Integer = iTimerToEndOfWalk
            iTimerToEndOfWalk = value

            ' If we've reached the end of the timer
            If Status = StatusEnum.Running AndAlso iTimerToEndOfWalk = 0 Then
                lStop(True, True)
            End If

        End Set
    End Property

    Private LastSubWalk As SubWalk
    Private ReadOnly Property TimerFromLastSubWalk() As Integer
        Get
            Return TimerFromStartOfWalk - iLastSubWalkTime
        End Get
    End Property
    Private ReadOnly Property TimerFromStartOfWalk() As Integer
        Get
            Return Length - TimerToEndOfWalk '+ 1
        End Get
    End Property


    Public bJustStarted As Boolean = False
    Public Sub Start(Optional ByVal bForce As Boolean = False)
        If bForce Then
            lStart()
        Else
            If NextCommand = Command.Stop Then
                NextCommand = Command.Restart
            Else
                NextCommand = Command.Start
            End If
        End If
    End Sub
    Public Sub lStart(Optional ByVal bRestart As Boolean = False)
        If Status = StatusEnum.NotYetStarted OrElse Status = StatusEnum.Finished OrElse (Status = StatusEnum.Running AndAlso bRestart) Then
            If Not bRestart Then UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Low, "Starting walk " & Description)
            Status = StatusEnum.Running
            ResetLength()
            TimerToEndOfWalk = Length

            If TimerFromStartOfWalk = 0 Then
                DoAnySteps()
                DoAnySubWalks() ' To run 'after 0 turns' subevents
            End If

            bJustStarted = True
        Else
            UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Error, "Can't Start a Walk that isn't waiting!")
        End If
    End Sub


    Public Sub Pause()
        NextCommand = Command.Pause
    End Sub
    Public Sub lPause()
        If Status = StatusEnum.Running Then
            UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Low, "Pausing walk " & Description)
            Status = StatusEnum.Paused
        Else
            UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Error, "Can't Pause a Walk that isn't running!")
        End If
    End Sub


    Public sTriggeringTask As String

    Public Sub [Resume]()
        NextCommand = Command.Resume
    End Sub
    Public Sub lResume()
        If Status = StatusEnum.Paused Then
            UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Low, "Resuming walk " & Description)
            Status = StatusEnum.Running
        Else
            UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Error, "Can't Resume a Walk that isn't paused!")
        End If
    End Sub


    Public Sub [Stop]()
        NextCommand = Command.Stop
    End Sub
    Public Sub lStop(Optional ByVal bRunSubEvents As Boolean = False, Optional ByVal bReachedEnd As Boolean = False)

        If bRunSubEvents Then DoAnySubWalks()
        Status = StatusEnum.Finished
        If Me.bLoop AndAlso TimerToEndOfWalk = 0 AndAlso bReachedEnd Then
            If Length > 0 Then ' Only restart if walk comes to and end and it is set to loop - not if it is terminated by task change
                UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Low, "Restarting walk " & Description)
                lStart(True)
            Else
                UserSession.DebugPrint(ItemEnum.Event, sKey, DebugDetailLevelEnum.Low, "Not restarting walk " & Description & " otherwise we'd get in an infinite loop as zero length.")
            End If
        Else
            UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Low, "Finishing walk " & Description)
        End If

    End Sub


    Public Sub IncrementTimer()
        If NextCommand <> Command.Nothing Then
            Select Case NextCommand
                Case Command.Start
                    lStart()
                Case Command.Stop
                    lStop()
                Case Command.Pause
                    lPause()
                Case Command.Resume
                    lResume()
                Case Command.Restart
                    lStart(True)
            End Select
            NextCommand = Command.Nothing
            sTriggeringTask = ""
        End If

        If Status = StatusEnum.Running Then UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.High, "Walk " & Description & " [" & TimerFromStartOfWalk + 1 & "/" & Length & "]")

        ' Split this into 2 case statements, as changing timer here may change status
        Select Case Status
            Case StatusEnum.NotYetStarted
            Case StatusEnum.Running
                If Not bJustStarted Then TimerToEndOfWalk -= 1
            Case StatusEnum.Paused
            Case StatusEnum.Finished
        End Select

        If Not bJustStarted Then
            DoAnySteps()
            DoAnySubWalks()
        End If

        bJustStarted = False

    End Sub


    Public Sub DoAnySteps()
        If Status = StatusEnum.Running Then
            Dim iStepLength As Integer = 0
            For Each [step] As clsStep In arlSteps
                If iStepLength = TimerFromStartOfWalk Then
                    Dim sDestination As String = ""
                    Dim sDestKey As String = [step].sLocation
                    If sDestKey = THEPLAYER Then sDestKey = Adventure.Player.Key
                    If Adventure.htblGroups.ContainsKey(sDestKey) Then
                        ' Get an adjacent location in the group
                        Dim grp As clsGroup = Adventure.htblGroups(sDestKey)
                        Dim ch As clsCharacter = Adventure.htblCharacters(sKey)
                        Dim locCurrent As clsLocation = Nothing
                        If ch.Location.ExistWhere <> clsCharacterLocation.ExistsWhereEnum.Hidden Then locCurrent = Adventure.htblLocations(ch.Location.LocationKey)
                        Dim bHasAdjacent As Boolean = False
                        If locCurrent IsNot Nothing Then
                            For Each sLoc As String In grp.arlMembers
                                If locCurrent.IsAdjacent(sLoc) Then
                                    bHasAdjacent = True
                                    Exit For
                                End If
                            Next
                        End If
                        If bHasAdjacent Then
                            While sDestination = ""
                                Dim sPossibleDest As String = grp.arlMembers(Random(grp.arlMembers.Count - 1))
                                If ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.Hidden OrElse locCurrent.IsAdjacent(sPossibleDest) Then sDestination = sPossibleDest
                            End While
                        Else
                            ' No adjacent room, so just move to a random room in the group
                            sDestination = grp.arlMembers(Random(grp.arlMembers.Count - 1))
                        End If
                        UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Medium, "Character " & Adventure.htblCharacters(sKey).ProperName & " walks to " & Adventure.GetNameFromKey(sDestKey) & " (" & Adventure.htblLocations(sDestination).ShortDescription.ToString & ")")
                    ElseIf Adventure.htblCharacters.ContainsKey(sDestKey) Then
                        ' Only move towards the character if they are in an adjacent room
                        Dim ch As clsCharacter = Adventure.htblCharacters(sKey)
                        Dim ch2 As clsCharacter = Adventure.htblCharacters(sDestKey)

                        If ch.Location.LocationKey <> ch2.Location.LocationKey Then
                            Dim locCurrent As clsLocation = Nothing
                            If ch.Location.ExistWhere <> clsCharacterLocation.ExistsWhereEnum.Hidden Then locCurrent = Adventure.htblLocations(ch.Location.LocationKey)
                            If locCurrent IsNot Nothing AndAlso locCurrent.IsAdjacent(ch2.Location.Key) Then
                                sDestination = ch2.Location.Key
                                UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Medium, "Character " & Adventure.htblCharacters(sKey).ProperName & " walks to " & ch2.ProperName & " (" & Adventure.htblLocations(sDestination).ShortDescription.ToString & ")")
                            Else
                                ' Character is not adjacent, so don't move
                                UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Error, "Character " & Adventure.htblCharacters(sKey).ProperName & " can't walk to " & ch2.ProperName & " as " & ch2.Gender.ToString.Replace("Male", "he").Replace("Female", "she").Replace("Unknown", "it") & " is not in an adjacent location.")
                                sDestination = ""
                            End If
                        End If
                    Else
                        sDestination = sDestKey
                        UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Medium, "Character " & Adventure.htblCharacters(sKey).ProperName & " walks to " & Adventure.GetNameFromKey(sDestination))
                    End If

                    If sDestination = HIDDEN OrElse Adventure.htblLocations.ContainsKey(sDestination) Then
                        With Adventure.htblCharacters(sKey)
                            If .HasProperty("ShowEnterExit") AndAlso .Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation Then
                                If .Location.Key = Adventure.Player.Location.LocationKey Then
                                    Dim sExits As String = "exits"
                                    If .HasProperty("CharExits") Then sExits = .GetPropertyValue("CharExits") '.StringData.ToString
                                    Dim sLeaves As String = ToProper(.Name, False) & " " & sExits ' to the North... etc
                                    If Adventure.htblLocations(Adventure.Player.Location.LocationKey).IsAdjacent(sDestination) Then
                                        Dim sTheDirection As String = Adventure.htblLocations(.Location.LocationKey).DirectionTo(sDestination)
                                        If sTheDirection <> "nowhere" Then
                                            Select Case sTheDirection
                                                Case "outside", "inside"
                                                    sLeaves &= " "
                                                Case Else
                                                    sLeaves &= " to "
                                            End Select
                                            sLeaves &= sTheDirection
                                        End If
                                    End If
                                    sLeaves &= "."
                                    UserSession.Display(sLeaves)
                                ElseIf sDestination = Adventure.Player.Location.LocationKey Then
                                    Dim sEnters As String = "enters"
                                    If .HasProperty("CharEnters") Then sEnters = .GetPropertyValue("CharEnters") '.StringData.ToString
                                    Dim sArrives As String = ToProper(.Name, False) & " " & sEnters ' from the North... etc
                                    If Adventure.htblLocations(Adventure.Player.Location.LocationKey).IsAdjacent(.Location.Key) Then
                                        Dim sTheDirection As String = Adventure.htblLocations(sDestination).DirectionTo(.Location.LocationKey)
                                        If sTheDirection <> "nowhere" Then sArrives &= " from " & sTheDirection
                                    End If
                                    sArrives &= "."
                                    UserSession.Display(sArrives)
                                End If
                            End If

                            .Move(sDestination)
                        End With
                    End If
                End If
                iStepLength += [step].ftTurns.Value
            Next
        End If

    End Sub


    Public Sub DoAnySubWalks()

        Select Case Status
            Case StatusEnum.Running
                ' Check all the subevents to see if we need to do anything
                Dim iIndex As Integer = 0
                For Each sw As SubWalk In SubWalks
                    Dim bRunSubWalk As Boolean = False
                    Select Case sw.eWhen
                        Case SubWalk.WhenEnum.FromStartOfWalk
                            If TimerFromStartOfWalk = sw.ftTurns.Value AndAlso sw.ftTurns.Value <= Length Then bRunSubWalk = True
                        Case SubWalk.WhenEnum.FromLastSubWalk
                            If TimerFromLastSubWalk = sw.ftTurns.Value Then
                                If (LastSubWalk Is Nothing AndAlso iIndex = 0) OrElse (iIndex > 0 AndAlso LastSubWalk Is SubWalks(iIndex - 1)) Then bRunSubWalk = True
                            End If
                        Case SubWalk.WhenEnum.BeforeEndOfWalk
                            If TimerToEndOfWalk = sw.ftTurns.Value Then bRunSubWalk = True
                        Case SubWalk.WhenEnum.ComesAcross
                            Dim bPrevSameLocationAsChar As Boolean = sw.bSameLocationAsChar
                            sw.bSameLocationAsChar = (Adventure.htblCharacters(sKey).Location.LocationKey = Adventure.Player.Location.LocationKey)

                            If Not bPrevSameLocationAsChar AndAlso sw.bSameLocationAsChar Then
                                bRunSubWalk = True
                            End If
                    End Select

                    If bRunSubWalk Then
                        Select Case sw.eWhat
                            Case SubWalk.WhatEnum.DisplayMessage
                                If sw.sKey3 <> "" AndAlso Adventure.Player.IsInGroupOrLocation(sw.sKey3) Then UserSession.Display(sw.oDescription.ToString)
                            Case SubWalk.WhatEnum.ExecuteTask
                                If Adventure.htblTasks.ContainsKey(sw.sKey2) Then
                                    UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Medium, "Walk '" & Description & "' attempting to execute task '" & Adventure.htblTasks(sw.sKey2).Description & "'")
                                    UserSession.AttemptToExecuteTask(sw.sKey2, True)
                                End If
                            Case SubWalk.WhatEnum.UnsetTask
                                UserSession.DebugPrint(ItemEnum.Character, sKey, DebugDetailLevelEnum.Medium, "Walk '" & Description & "' unsetting task '" & Adventure.htblTasks(sw.sKey2).Description & "'")
                                Adventure.htblTasks(sw.sKey2).Completed = False
                        End Select
                        iLastSubWalkTime = TimerFromStartOfWalk
                        LastSubWalk = sw
                    End If
                    iIndex += 1
                Next
        End Select

    End Sub

End Class



Public Class clsCharacterLocation

    Private Parent As clsCharacter
    Public Sub New(ByVal Parent As clsCharacter)
        If Parent IsNot Nothing AndAlso Parent.Location IsNot Nothing Then sLastLocationKey = Parent.Location.sLastLocationKey
        Me.Parent = Parent
    End Sub

    Public Enum ExistsWhereEnum
        Uninitialised
        Hidden
        AtLocation
        OnObject
        InObject
        OnCharacter
    End Enum

    Private eExistsWhere As ExistsWhereEnum = ExistsWhereEnum.Uninitialised
    Public Property ExistWhere() As ExistsWhereEnum
        Get
            If eExistsWhere = ExistsWhereEnum.Uninitialised Then
                If Parent.HasProperty("CharacterLocation") Then
                    Select Case Parent.GetPropertyValue("CharacterLocation")
                        Case "At Location"
                            eExistsWhere = ExistsWhereEnum.AtLocation
                        Case "Hidden"
                            eExistsWhere = ExistsWhereEnum.Hidden
                        Case "In Object"
                            eExistsWhere = ExistsWhereEnum.InObject
                        Case "On Character"
                            eExistsWhere = ExistsWhereEnum.OnCharacter
                        Case "On Object"
                            eExistsWhere = ExistsWhereEnum.OnObject
                        Case Else
                            ' Hmmm....
                    End Select
                Else
                    eExistsWhere = ExistsWhereEnum.Hidden
                End If
            End If
            Return eExistsWhere
        End Get
        Set(ByVal value As ExistsWhereEnum)
            If value <> eExistsWhere Then
                Dim p As clsProperty

                If Not Parent.HasProperty("CharacterLocation") Then
                    p = Adventure.htblAllProperties("CharacterLocation").Copy
                    p.Selected = True
                    Parent.AddProperty(p)
                End If

                Dim sNewLocation As String = ""
                Select Case value
                    Case ExistsWhereEnum.AtLocation
                        sNewLocation = "At Location"
                    Case ExistsWhereEnum.Hidden
                        sNewLocation = "Hidden"
                    Case ExistsWhereEnum.InObject
                        sNewLocation = "In Object"
                    Case ExistsWhereEnum.OnCharacter
                        sNewLocation = "On Character"
                    Case ExistsWhereEnum.OnObject
                        sNewLocation = "On Object"
                End Select
                Parent.SetPropertyValue("CharacterLocation", sNewLocation)

                For Each sProp As String In New String() {"CharacterAtLocation", "CharInsideWhat", "CharOnWhat", "CharOnWho"}
                    If Parent.HasProperty(sProp) Then Parent.RemoveProperty(sProp)
                Next

                If value <> ExistsWhereEnum.Hidden Then
                    Dim sNewProp As String = ""
                    Select Case value
                        Case ExistsWhereEnum.AtLocation
                            sNewProp = "CharacterAtLocation"
                        Case ExistsWhereEnum.InObject
                            sNewProp = "CharInsideWhat"
                        Case ExistsWhereEnum.OnCharacter
                            sNewProp = "CharOnWho"
                        Case ExistsWhereEnum.OnObject
                            sNewProp = "CharOnWhat"
                    End Select
                    If Not Parent.HasProperty(sNewProp) AndAlso Adventure.htblAllProperties.ContainsKey(sNewProp) Then
                        p = Adventure.htblAllProperties(sNewProp).Copy
                        p.Selected = True
                        Parent.AddProperty(p)
                    End If
                End If

                eExistsWhere = value
            End If
        End Set
    End Property

    Public Enum PositionEnum
        Uninitialised
        Standing
        Sitting
        Lying
    End Enum
    Private ePosition As PositionEnum = PositionEnum.Uninitialised

    Private sKey As String = Nothing
    Public Property Key() As String ' This could be key of location, object or character
        Get
            If sKey Is Nothing Then
                Select Case ExistWhere
                    Case ExistsWhereEnum.AtLocation
                        If Parent.HasProperty("CharacterAtLocation") Then sKey = Parent.GetPropertyValue("CharacterAtLocation")
                    Case ExistsWhereEnum.Hidden
                        sKey = ""
                    Case ExistsWhereEnum.InObject
                        If Parent.HasProperty("CharInsideWhat") Then sKey = Parent.GetPropertyValue("CharInsideWhat")
                    Case ExistsWhereEnum.OnCharacter
                        If Parent.HasProperty("CharOnWho") Then sKey = Parent.GetPropertyValue("CharOnWho")
                        If sKey = THEPLAYER Then sKey = Adventure.Player.Key
                    Case ExistsWhereEnum.OnObject
                        If Parent.HasProperty("CharOnWhat") Then sKey = Parent.GetPropertyValue("CharOnWhat")
                End Select
            End If
            Return sKey
        End Get
        Set(ByVal Value As String)
            sKey = Value
            Select Case ExistWhere
                Case ExistsWhereEnum.AtLocation
                    If Parent.HasProperty("CharacterAtLocation") Then Parent.SetPropertyValue("CharacterAtLocation", Value)
                Case ExistsWhereEnum.InObject
                    If Parent.HasProperty("CharInsideWhat") Then Parent.SetPropertyValue("CharInsideWhat", Value)
                Case ExistsWhereEnum.OnCharacter
                    If Parent.HasProperty("CharOnWho") Then Parent.SetPropertyValue("CharOnWho", Value)
                Case ExistsWhereEnum.OnObject
                    If Parent.HasProperty("CharOnWhat") Then Parent.SetPropertyValue("CharOnWhat", Value)
            End Select
        End Set
    End Property


    Friend sLastLocationKey As String
    ' Returns the key of the location that the character is ultimately in
    Public ReadOnly Property LocationKey() As String
        Get
            LocationKey = ""
            Try
                Select Case ExistWhere
                    Case ExistsWhereEnum.AtLocation
                        Return Key
                    Case ExistsWhereEnum.Hidden
                        ' Hmm
                        Return HIDDEN
                    Case ExistsWhereEnum.InObject, ExistsWhereEnum.OnObject
                        Dim ob As clsObject = Nothing
                        If Adventure.htblObjects.TryGetValue(Key, ob) Then
                            If sLastLocationKey <> "" AndAlso ob.LocationRoots.ContainsKey(sLastLocationKey) Then
                                Return sLastLocationKey
                            Else
                                For Each sLocKey As String In ob.LocationRoots.Keys
                                    Return sLocKey
                                Next
                            End If
                        End If
                        Return ""
                    Case ExistsWhereEnum.OnCharacter
                        Return Adventure.htblCharacters(Key).Location.LocationKey
                    Case Else
                        Return ""
                End Select
            Catch ex As Exception
                ErrMsg("LocationKey error", ex)
            Finally
                If LocationKey <> "" AndAlso LocationKey <> sLastLocationKey Then sLastLocationKey = LocationKey
            End Try

        End Get
    End Property


    Public Sub ResetPosition()
        ePosition = PositionEnum.Uninitialised
    End Sub

    Public Property Position() As PositionEnum
        Get
            If ePosition = PositionEnum.Uninitialised Then
                If Parent.HasProperty("CharacterPosition") Then
                    Select Case Parent.GetPropertyValue("CharacterPosition")
                        Case "Standing"
                            ePosition = PositionEnum.Standing
                        Case "Sitting"
                            ePosition = PositionEnum.Sitting
                        Case "Lying"
                            ePosition = PositionEnum.Lying
                        Case Else
                            ' Hmmm....
                    End Select
                Else
                    ePosition = PositionEnum.Standing
                End If
            End If
            Return ePosition
        End Get
        Set(ByVal value As PositionEnum)
            If value <> ePosition Then
                Parent.SetPropertyValue("CharacterPosition", value.ToString)

                ePosition = value
            End If
        End Set

    End Property


    Public Shadows ReadOnly Property ToString() As String
        Get
            If ExistWhere = ExistsWhereEnum.Hidden Then Return "Hidden"

            Dim sWhere As String = ""

            Select Case Position
                Case PositionEnum.Standing
                    sWhere = "Standing "
                Case PositionEnum.Sitting
                    sWhere = "Sitting "
                Case PositionEnum.Lying
                    sWhere = "Lying "
            End Select

            Select Case ExistWhere
                Case ExistsWhereEnum.AtLocation
                    sWhere = "at " & Adventure.htblLocations(Key).ShortDescription.ToString
                Case ExistsWhereEnum.OnObject
                    sWhere &= "on " & Adventure.htblObjects(Key).FullName
                Case ExistsWhereEnum.InObject
                    sWhere &= "in " & Adventure.htblObjects(Key).FullName
                Case ExistsWhereEnum.OnCharacter
                    sWhere &= "on " & Adventure.htblCharacters(Key).Name(, False)
            End Select

            Return sWhere
        End Get
    End Property

End Class
