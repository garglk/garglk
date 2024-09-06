Public Class clsObject
    Inherits clsItemWithProperties

    Private sPrefix As String
    Private oDescription As Description
    Private cLocation As clsObjectLocation
    Private sArticle As String

    Friend m_arlNames As New StringArrayList
    Friend Property arlNames As StringArrayList
        Get
            Return m_arlNames
        End Get
        Set(value As StringArrayList)
            m_arlNames = value
        End Set
    End Property

    Private sarlPlurals As StringArrayList
    Friend ReadOnly Property arlPlurals() As StringArrayList
        Get
            If sarlPlurals Is Nothing Then
                Dim arl As New StringArrayList

                For Each sNoun As String In arlNames
                    'arl.Add(sNoun & "|" & GuessPluralFromNoun(sNoun)) ' This means we're matching non-plurals as plurals, so "get ball" will be the same as "get balls" and therefore take all balls without disambiguation
                    arl.Add(GuessPluralFromNoun(sNoun))
                Next
                sarlPlurals = arl
            End If

            Return sarlPlurals
        End Get
    End Property

    Friend Function GuessPluralFromNoun(ByVal sNoun As String) As String
        If sNoun Is Nothing Then Return ""

        Select Case sNoun
            Case "deer", "fish", "cod", "mackerel", "trout", "moose", "sheep", "swine", "aircraft", "blues", "cannon" ' Identical Singular & Plural nouns
                Return sNoun
            Case "ox" ' Irregular plurals
                Return "oxen"
            Case "cow"
                Return "kine"
            Case "child"
                Return "children"
            Case "foot" ' Umlaut plurals
                Return "feet"
            Case "goose"
                Return "geese"
            Case "louse"
                Return "lice"
            Case "mouse"
                Return "mice"
            Case "tooth"
                Return "teeth"
        End Select

        Select Case sNoun.Length
            Case 0
                Return ""
            Case 1, 2
                Return sNoun & "s"
            Case Else
                Select Case sNoun.Substring(sNoun.Length - 3, 3)
                    Case "man" ' Umlaut plural
                        Return sNoun.Substring(0, sNoun.Length - 2) & "en"
                    Case "ies"
                        Return sNoun
                End Select
                Select Case sNoun.Substring(sNoun.Length - 2, 2)
                    Case "sh", "ss", "ch" ' Sibilant sounds
                        Return sNoun & "es"
                    Case "ge", "se" ' Sibilant sounds, ending with 'e'
                        Return sNoun & "s"
                    Case "ex"
                        Return sNoun.Substring(0, sNoun.Length - 2) & "ices"
                    Case "is"
                        Return sNoun.Substring(0, sNoun.Length - 2) & "es"
                    Case "um"
                        Return sNoun.Substring(0, sNoun.Length - 2) & "a"
                    Case "us"
                        Return sNoun.Substring(0, sNoun.Length - 2) & "i"
                End Select
                Select Case sNoun.Substring(sNoun.Length - 1, 1)
                    Case "f"
                        Select Case sNoun
                            Case "dwarf", "hoof", "roof"  ' the exceptions
                                Return (sNoun & "s")
                            Case Else
                                Return sNoun.Substring(0, sNoun.Length - 1) & "ves"
                        End Select
                    Case "o" ' nouns ending in 'o' preceded by a consonant
                        Select Case sNoun.Substring(sNoun.Length - 2, 1)
                            Case "b", "c", "d", "f", "g", "h", "j", "k", "l", "m", "n", "p", "q", "r", "s", "t", "v", "w", "x", "z"
                                Return sNoun & "es"
                        End Select
                    Case "x" ' normally ends in "es"
                        Return sNoun & "es"
                    Case "y" ' nouns ending in y preceeded by a consonant usually drop y and add ies
                        Select Case sNoun.Substring(sNoun.Length - 2, 1)
                            Case "b", "c", "d", "f", "g", "h", "j", "k", "l", "m", "n", "p", "q", "r", "s", "t", "v", "w", "x", "z"
                                Return sNoun.Substring(0, sNoun.Length - 1) & "ies"
                        End Select
                End Select
        End Select

        If sNoun.EndsWith("s") Then
            Return sNoun
        Else
            Return sNoun & "s"
        End If

    End Function


    Private iIsStaticCache As Integer = Integer.MinValue
    Public Property IsStatic() As Boolean
        Get
            If iIsStaticCache <> Integer.MinValue Then
                Return CBool(iIsStaticCache)
            Else
                Return GetPropertyValue("StaticOrDynamic") <> "Dynamic"
            End If
        End Get
        Set(value As Boolean)
            SetPropertyValue("StaticOrDynamic", IIf(value, "Static", "Dynamic").ToString)
            iIsStaticCache = CInt(value)
        End Set
    End Property

    Public Property HasSurface() As Boolean
        Get
            Return GetPropertyValue("Surface") IsNot Nothing
        End Get
        Set(value As Boolean)
            TODO("Set HasSurface()")
        End Set
    End Property

    ' TODO - Enhance this, or make it user configuratble
    Public ReadOnly Property IsPlural As Boolean
        Get
            Return Article = "some"
        End Get
    End Property


    Public Property IsContainer() As Boolean
        Get
            Return GetPropertyValue("Container") IsNot Nothing
        End Get
        Set(value As Boolean)
            TODO("Set IsContainer()")
        End Set
    End Property

    Public Property IsWearable() As Boolean
        Get
            Return GetPropertyValue("Wearable") IsNot Nothing
        End Get
        Set(value As Boolean)
            SetPropertyValue("Wearable", value)
        End Set
    End Property


    Public Property ExplicitlyList() As Boolean
        Get
            Return HasProperty("ExplicitlyList")
        End Get
        Set(ByVal Value As Boolean)
            SetPropertyValue("ExplicitlyList", Value)
        End Set
    End Property


    Public Property ListDescription() As String
        Get
            If IsStatic Then
                Return GetPropertyValue("ListDescription")
            Else
                Return GetPropertyValue("ListDescriptionDynamic")
            End If
            Return ""
        End Get
        Set(ByVal Value As String)
            If IsStatic Then
                SetPropertyValue("ListDescription", Value)
            Else
                SetPropertyValue("ListDescriptionDynamic", Value)
            End If
        End Set
    End Property


    Public Property ExplicitlyExclude() As Boolean
        Get
            Return HasProperty("ExplicitlyExclude")
        End Get
        Set(ByVal Value As Boolean)
            SetPropertyValue("ExplicitlyExclude", Value)
        End Set
    End Property



    Public Property IsLieable() As Boolean
        Get
            Return HasProperty("Lieable")
        End Get
        Set(ByVal Value As Boolean)
            SetPropertyValue("Lieable", Value)
        End Set
    End Property


    Public Property IsSittable() As Boolean
        Get
            Return HasProperty("Sittable")
        End Get
        Set(ByVal Value As Boolean)
            SetPropertyValue("Sittable", Value)
        End Set
    End Property


    Public Property IsStandable() As Boolean
        Get
            Return HasProperty("Standable")
        End Get
        Set(ByVal Value As Boolean)
            SetPropertyValue("Standable", Value)
        End Set
    End Property

    ' Returns True if object is directly on parent, or if on/in something that is on it
    Public ReadOnly Property IsOn(ByVal sParentKey As String) As Boolean
        Get
            If (Me.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject OrElse Me.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject) AndAlso Parent <> "" Then
                If Me.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject AndAlso Parent = sParentKey Then
                    Return True
                Else
                    Dim obParent As clsObject = Adventure.htblObjects(Parent)
                    Return obParent.IsOn(sParentKey)
                End If
            Else
                Return False
            End If
        End Get
    End Property

    ' Returns True if object is directly inside parent, or if on/in something that is inside it
    Public ReadOnly Property IsInside(ByVal sParentKey As String) As Boolean
        Get
            If (Me.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject OrElse Me.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject) AndAlso Parent <> "" Then
                If Me.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject AndAlso Parent = sParentKey Then
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

    Public ReadOnly Property ExistsAtLocation(ByVal sLocKey As String, Optional ByVal bDirectly As Boolean = False) As Boolean
        Get
            If IsStatic Then
                Select Case Location.StaticExistWhere
                    Case clsObjectLocation.StaticExistsWhereEnum.AllRooms
                        Return True
                    Case clsObjectLocation.StaticExistsWhereEnum.NoRooms
                        Return sLocKey = HIDDEN
                    Case clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter
                        If bDirectly Then Return False Else Return Adventure.htblCharacters(Location.Key).Location.Key = sLocKey
                    Case clsObjectLocation.StaticExistsWhereEnum.PartOfObject
                        If bDirectly Then Return False Else Return Adventure.htblObjects(Location.Key).ExistsAtLocation(sLocKey)
                    Case clsObjectLocation.StaticExistsWhereEnum.LocationGroup
                        Return Adventure.htblGroups(Location.Key).arlMembers.Contains(sLocKey)
                    Case clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                        Return sLocKey = Location.Key
                End Select
            Else
                Select Case Location.DynamicExistWhere
                    Case clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter
                        If bDirectly Then Return False Else Return Adventure.htblCharacters(Location.Key).Location.Key = sLocKey
                    Case clsObjectLocation.DynamicExistsWhereEnum.Hidden
                        Return sLocKey = HIDDEN
                    Case clsObjectLocation.DynamicExistsWhereEnum.InLocation
                        Return sLocKey = Location.Key
                    Case clsObjectLocation.DynamicExistsWhereEnum.InObject
                        If bDirectly Then Return False Else Return Adventure.htblObjects(Location.Key).ExistsAtLocation(sLocKey)
                    Case clsObjectLocation.DynamicExistsWhereEnum.OnObject
                        If bDirectly Then Return False Else Return Adventure.htblObjects(Location.Key).ExistsAtLocation(sLocKey)
                    Case clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                        If bDirectly Then Return False Else Return Adventure.htblCharacters(Location.Key).Location.Key = sLocKey
                End Select
            End If
        End Get
    End Property

    Public Property IsTransparent As Boolean
        Get
            Return False ' TODO
        End Get
        Set(value As Boolean)
            TODO()
        End Set
    End Property

    Public Property IsOpen() As Boolean
        Get
            Dim s As String = GetPropertyValue("OpenStatus")
            Return s Is Nothing OrElse s = "Open"
        End Get
        Set(ByVal Value As Boolean)
            SetPropertyValue("OpenStatus", IIf(Value, "Open", "Closed").ToString)
        End Set
    End Property
    Friend ReadOnly Property FullName(Optional ByVal Article As ArticleTypeEnum = ArticleTypeEnum.Indefinite) As String
        Get
            If arlNames.Count > 0 Then
                Dim sArticle2 As String = Nothing
                Select Case Article
                    Case ArticleTypeEnum.Definite
                        sArticle2 = "the "
                    Case ArticleTypeEnum.Indefinite
                        sArticle2 = sArticle & " "
                    Case ArticleTypeEnum.None
                        sArticle2 = ""
                End Select
                If sPrefix <> "" Then
                    Return sArticle2 & sPrefix & " " & arlNames(0)
                Else
                    Return sArticle2 & arlNames(0)
                End If
            Else
                Return "Undefined Object"
            End If
        End Get
    End Property
    Friend Property Article() As String
        Get
            Return sArticle
        End Get
        Set(ByVal Value As String)
            sArticle = Value
        End Set
    End Property

    Friend Property Prefix() As String
        Get
            Return sPrefix
        End Get
        Set(ByVal Value As String)
            sPrefix = Value
        End Set
    End Property

    Public Function DisplayCharacterChildren() As String

        Dim sReturn As String = ""

        If ChildrenCharacters(WhereChildrenEnum.OnObject).Count > 0 Then
            sReturn = pSpace(sReturn)
            If sReturn = "" Then
                sReturn &= "%PCase[" & ChildrenCharacters(WhereChildrenEnum.OnObject).List("and") & "]%"
            Else
                sReturn &= ChildrenCharacters(WhereChildrenEnum.OnObject).List("and")
            End If            
            If ChildrenCharacters(WhereChildrenEnum.OnObject).Count = 1 Then
                sReturn &= " [am/are/is] on "
            Else
                sReturn &= " are on "
            End If
            sReturn &= FullName(ArticleTypeEnum.Definite)
        End If
        If Not Openable OrElse IsOpen Then
            If ChildrenCharacters(WhereChildrenEnum.InsideObject).Count > 0 Then
                If ChildrenCharacters(WhereChildrenEnum.OnObject).Count > 0 Then
                    sReturn &= ", and "
                    sReturn &= ChildrenCharacters(WhereChildrenEnum.InsideObject).List("and")
                Else
                    sReturn = pSpace(sReturn)
                    If sReturn = "" Then
                        sReturn &= "%PCase[" & ChildrenCharacters(WhereChildrenEnum.InsideObject).List("and") & "]%"
                    Else
                        sReturn &= ChildrenCharacters(WhereChildrenEnum.InsideObject).List("and")
                    End If
                End If
                If ChildrenCharacters(WhereChildrenEnum.InsideObject).Count = 1 Then
                    sReturn &= " [am/are/is] "
                Else
                    sReturn &= " are "
                End If
                sReturn &= "inside " & FullName(ArticleTypeEnum.Definite)
            End If
        End If
        If ChildrenCharacters(WhereChildrenEnum.OnObject).Count > 0 OrElse (((Openable AndAlso IsOpen) OrElse Not Openable) AndAlso ChildrenCharacters(WhereChildrenEnum.InsideObject).Count > 0) Then sReturn &= "."

        Return sReturn
    End Function



    Public Function DisplayObjectChildren() As String
        Dim sReturn As String = ""

        If Children(WhereChildrenEnum.OnObject).Count > 0 Then
            sReturn = pSpace(sReturn)
            sReturn &= ToProper(Children(WhereChildrenEnum.OnObject).List("and", False, ArticleTypeEnum.Indefinite))
            If Children(WhereChildrenEnum.OnObject).Count = 1 Then
                sReturn &= " is on "
            Else
                sReturn &= " are on "
            End If
            sReturn &= FullName(ArticleTypeEnum.Definite)
        End If
        If (Openable AndAlso IsOpen) OrElse Not Openable Then
            If Children(WhereChildrenEnum.InsideObject).Count > 0 Then
                If Children(WhereChildrenEnum.OnObject).Count > 0 Then
                    sReturn &= ", and inside"
                Else
                    sReturn = pSpace(sReturn)
                    sReturn &= "Inside " & FullName(ArticleTypeEnum.Definite)
                End If
                If Children(WhereChildrenEnum.InsideObject).Count = 1 Then
                    sReturn &= " is "
                Else
                    sReturn &= " are "
                End If
                sReturn &= Children(WhereChildrenEnum.InsideObject).List("and", False, ArticleTypeEnum.Indefinite)
            End If
        End If
        If Children(WhereChildrenEnum.OnObject).Count > 0 OrElse (((Openable AndAlso IsOpen) OrElse Not Openable) AndAlso Children(WhereChildrenEnum.InsideObject).Count > 0) Then sReturn &= "."

        If sReturn = "" Then sReturn = "Nothing is on or inside " & FullName(ArticleTypeEnum.Definite) & "."
        Return sReturn
    End Function


    Friend Property Description() As Description
        Get
            If oDescription.ToString(True) = "" Then
                oDescription(0).Description = "There is nothing special about " & FullName(ArticleTypeEnum.Definite) & "."
            End If
            Return oDescription
        End Get
        Set(ByVal Value As Description)
            oDescription = Value
        End Set
    End Property


    ' Returns the actual rooms an object is in, regardless of actual location
    Friend ReadOnly Property LocationRoots() As LocationHashTable
        Get
            Dim htblLocRoot As New LocationHashTable

            If Me.IsStatic Then
                Select Case Location.StaticExistWhere
                    Case clsObjectLocation.StaticExistsWhereEnum.AllRooms
                        Return Adventure.htblLocations
                    Case clsObjectLocation.StaticExistsWhereEnum.NoRooms
                        ' Fall out
                    Case clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter
                        If Adventure.htblCharacters.ContainsKey(Location.Key) Then ' We could be loading...
                            Dim locChar As clsCharacterLocation = Adventure.htblCharacters(Location.Key).Location
                            If locChar.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation Then htblLocRoot.Add(Adventure.htblLocations(locChar.Key), locChar.Key)
                        End If
                        Return htblLocRoot

                    Case clsObjectLocation.StaticExistsWhereEnum.PartOfObject
                        Return Adventure.htblObjects(Location.Key).LocationRoots

                    Case clsObjectLocation.StaticExistsWhereEnum.LocationGroup
                        For Each sMemberKey As String In Adventure.htblGroups(Location.Key).arlMembers
                            htblLocRoot.Add(Adventure.htblLocations(sMemberKey), sMemberKey)
                        Next

                    Case clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                        htblLocRoot.Add(Adventure.htblLocations(Location.Key), Location.Key)

                End Select
            Else
                Select Case Location.DynamicExistWhere
                    Case clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter
                        Dim locChar As clsCharacterLocation = Adventure.htblCharacters(Location.Key).Location
                        If locChar.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation Then htblLocRoot.Add(Adventure.htblLocations(locChar.Key), locChar.Key)
                        Return htblLocRoot

                    Case clsObjectLocation.DynamicExistsWhereEnum.Hidden
                        ' Fall out

                    Case clsObjectLocation.DynamicExistsWhereEnum.InLocation
                        If Location.Key IsNot Nothing AndAlso Location.Key <> "Hidden" Then htblLocRoot.Add(Adventure.htblLocations(Location.Key), Location.Key)

                    Case clsObjectLocation.DynamicExistsWhereEnum.InObject
                        Return Adventure.htblObjects(Location.Key).LocationRoots

                    Case clsObjectLocation.DynamicExistsWhereEnum.OnObject
                        Return Adventure.htblObjects(Location.Key).LocationRoots

                    Case clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                        Dim locChar As clsCharacterLocation = Adventure.htblCharacters(Location.Key).Location
                        If locChar.ExistWhere = clsCharacterLocation.ExistsWhereEnum.AtLocation Then htblLocRoot.Add(Adventure.htblLocations(locChar.Key), locChar.Key)
                        Return htblLocRoot

                End Select
            End If

            Return htblLocRoot
        End Get
    End Property


    Public Property Location() As clsObjectLocation
        Get
            cLocation = New clsObjectLocation
            If Not Me.IsStatic AndAlso HasProperty("DynamicLocation") Then

                Select Case GetPropertyValue("DynamicLocation")
                    Case "Held By Character"
                        cLocation.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter
                        If HasProperty("HeldByWho") Then cLocation.Key = GetPropertyValue("HeldByWho")
                    Case "Hidden"
                        cLocation.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.Hidden
                    Case "In Location"
                        cLocation.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InLocation
                        If HasProperty("InLocation") Then cLocation.Key = GetPropertyValue("InLocation")
                    Case "Inside Object"
                        cLocation.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject
                        If HasProperty("InsideWhat") Then cLocation.Key = GetPropertyValue("InsideWhat")
                    Case "On Object"
                        cLocation.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject
                        If HasProperty("OnWhat") Then cLocation.Key = GetPropertyValue("OnWhat")
                    Case "Worn By Character"
                        cLocation.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                        If HasProperty("WornByWho") Then cLocation.Key = GetPropertyValue("WornByWho")
                End Select
            ElseIf Me.IsStatic AndAlso HasProperty("StaticLocation") Then
                Select Case GetPropertyValue("StaticLocation")
                    Case "Nowhere", "Hidden"
                        cLocation.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.NoRooms
                    Case "Single Location"
                        cLocation.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                        If HasProperty("AtLocation") Then cLocation.Key = GetPropertyValue("AtLocation")
                    Case "Location Group"
                        cLocation.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.LocationGroup
                        If HasProperty("AtLocationGroup") Then cLocation.Key = GetPropertyValue("AtLocationGroup")
                    Case "Everywhere"
                        cLocation.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.AllRooms
                    Case "Part of Character"
                        cLocation.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter
                        If HasProperty("PartOfWho") Then cLocation.Key = GetPropertyValue("PartOfWho")
                    Case "Part of Object"
                        cLocation.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.PartOfObject
                        If HasProperty("PartOfWhat") Then cLocation.Key = GetPropertyValue("PartOfWhat")
                End Select
            End If
            Return cLocation
        End Get

        Set(ByVal Value As clsObjectLocation)
            cLocation = Value
            If Not Me.IsStatic AndAlso HasProperty("DynamicLocation") Then
                Select Case cLocation.DynamicExistWhere
                    Case clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter
                        SetPropertyValue("DynamicLocation", "Held By Character")
                        AddProperty("HeldByWho")
                        SetPropertyValue("HeldByWho", cLocation.Key)
                    Case clsObjectLocation.DynamicExistsWhereEnum.Hidden
                        SetPropertyValue("DynamicLocation", "Hidden")
                    Case clsObjectLocation.DynamicExistsWhereEnum.InLocation
                        SetPropertyValue("DynamicLocation", "In Location")
                        AddProperty("InLocation")
                        SetPropertyValue("InLocation", cLocation.Key)
                    Case clsObjectLocation.DynamicExistsWhereEnum.InObject
                        SetPropertyValue("DynamicLocation", "Inside Object")
                        AddProperty("InsideWhat")
                        SetPropertyValue("InsideWhat", cLocation.Key)
                    Case clsObjectLocation.DynamicExistsWhereEnum.OnObject
                        SetPropertyValue("DynamicLocation", "On Object")
                        AddProperty("OnWhat")
                        SetPropertyValue("OnWhat", cLocation.Key)
                    Case clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                        SetPropertyValue("DynamicLocation", "Worn By Character")
                        AddProperty("WornByWho")
                        SetPropertyValue("WornByWho", cLocation.Key)
                End Select
            ElseIf Me.IsStatic AndAlso HasProperty("StaticLocation") Then
                Select Case cLocation.StaticExistWhere
                    Case clsObjectLocation.StaticExistsWhereEnum.AllRooms
                        SetPropertyValue("StaticLocation", "Everywhere")
                    Case clsObjectLocation.StaticExistsWhereEnum.LocationGroup
                        SetPropertyValue("StaticLocation", "Location Group")
                        AddProperty("AtLocationGroup")
                        SetPropertyValue("AtLocationGroup", cLocation.Key)
                    Case clsObjectLocation.StaticExistsWhereEnum.NoRooms
                        SetPropertyValue("StaticLocation", "Hidden")
                    Case clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter
                        SetPropertyValue("StaticLocation", "Part of Character")
                        AddProperty("PartOfWho")
                        SetPropertyValue("PartOfWho", cLocation.Key)
                    Case clsObjectLocation.StaticExistsWhereEnum.PartOfObject
                        SetPropertyValue("StaticLocation", "Part of Object")
                        AddProperty("PartOfWhat")
                        SetPropertyValue("PartOfWhat", cLocation.Key)
                    Case clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                        SetPropertyValue("StaticLocation", "Single Location")
                        AddProperty("AtLocation")
                        SetPropertyValue("AtLocation", cLocation.Key)
                End Select
            End If
        End Set
    End Property

    Public ReadOnly Property Openable() As Boolean
        Get
            Return HasProperty("Openable")
        End Get
    End Property

    Public ReadOnly Property Lockable() As Boolean
        Get
            Return HasProperty("Lockable")
        End Get
    End Property

    Public ReadOnly Property Readable() As Boolean
        Get
            Return HasProperty("Readable")
        End Get
    End Property

    Public Property ReadText() As String
        Get
            Return GetPropertyValue("ReadText")
        End Get
        Set(ByVal Value As String)
            SetPropertyValue("ReadText", Value)
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
            If IsStatic Then
                Select Case Location.StaticExistWhere
                    Case clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter, clsObjectLocation.StaticExistsWhereEnum.PartOfObject
                        Return Location.Key
                    Case clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                        Return Location.Key
                    Case clsObjectLocation.StaticExistsWhereEnum.NoRooms
                        Return HIDDEN
                    Case clsObjectLocation.StaticExistsWhereEnum.AllRooms
                        Return ALLROOMS
                    Case clsObjectLocation.StaticExistsWhereEnum.LocationGroup
                        Return Location.Key
                End Select
            Else
                Select Case Location.DynamicExistWhere
                    Case clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter, clsObjectLocation.DynamicExistsWhereEnum.InObject, clsObjectLocation.DynamicExistsWhereEnum.OnObject, clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                        Return Location.Key
                    Case clsObjectLocation.DynamicExistsWhereEnum.InLocation
                        Return Location.Key
                    Case clsObjectLocation.DynamicExistsWhereEnum.Hidden
                        Return HIDDEN
                End Select
            End If
            Return Nothing
        End Get
    End Property


    Public ReadOnly Property sRegularExpressionString(Optional ByVal bMyRE As Boolean = False, Optional ByVal bPlural As Boolean = False, Optional ByVal bPrefixMandatory As Boolean = False) As String
        Get
            Dim arl As StringArrayList = CType(IIf(bPlural, arlPlurals, arlNames), StringArrayList)

            If bMyRE Then
                ' The ADRIFT 'advanced command construction' expression
                Dim sRE As String = "{" & sArticle & "/the} "
                If sPrefix <> "" Then
                    For Each sSinglePrefix As String In Split(sPrefix, " ")
                        sRE &= "{" & sSinglePrefix & "} "
                    Next
                End If
                sRE &= "["

                For Each sNoun As String In arl
                    sRE &= sNoun & "/"
                Next
                Return Left(sRE, sRE.Length - 1) & "]"
            Else
                ' Real Regular Expressions
                Dim sRE As String = "(" & sArticle & " |the )?"
                For Each sSinglePrefix As String In Split(sPrefix, " ")
                    If sSinglePrefix <> "" Then sRE &= "(" & MakeTextRESafe(sSinglePrefix) & " )?"
                Next
                sRE &= "("
                For Each sNoun As String In arl
                    sRE &= MakeTextRESafe(sNoun) & "|"
                Next
                If arl.Count = 0 Then sRE &= "|" ' Fudge
                Return Left(sRE, sRE.Length - 1) & ")"
            End If
        End Get
    End Property

    Public Property SeenBy(ByVal sCharKey As String) As Boolean
        Get
            If sCharKey = "%Player%" Then sCharKey = Adventure.Player.Key
            Return Adventure.htblCharacters(sCharKey).HasSeenObject(Key)
        End Get
        Set(ByVal Value As Boolean)
            Adventure.htblCharacters(sCharKey).HasSeenObject(Key) = Value
        End Set
    End Property

    Public ReadOnly Property IsHeldByAnyone() As Boolean
        Get
            Select Case Location.DynamicExistWhere
                Case clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter
                    Return True
                Case clsObjectLocation.DynamicExistsWhereEnum.Hidden
                    Return False
                Case clsObjectLocation.DynamicExistsWhereEnum.InLocation
                    Return False
                Case clsObjectLocation.DynamicExistsWhereEnum.InObject
                    Return Adventure.htblObjects(Location.Key).IsHeldByAnyone
                Case clsObjectLocation.DynamicExistsWhereEnum.OnObject
                    Return Adventure.htblObjects(Location.Key).IsHeldByAnyone
                Case clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                    Return False
            End Select
        End Get
    End Property

    Public ReadOnly Property IsWornByAnyone() As Boolean
        Get
            Select Case Location.DynamicExistWhere
                Case clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter
                    Return False
                Case clsObjectLocation.DynamicExistsWhereEnum.Hidden
                    Return False
                Case clsObjectLocation.DynamicExistsWhereEnum.InLocation
                    Return False
                Case clsObjectLocation.DynamicExistsWhereEnum.InObject
                    Return False
                Case clsObjectLocation.DynamicExistsWhereEnum.OnObject
                    Return False
                Case clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                    Return True
            End Select
        End Get
    End Property

    ' The maximum container for the visibility of the object
    Public ReadOnly Property BoundVisible As String
        Get
            With Me.Location
                If Me.IsStatic Then
                    Select Case .StaticExistWhere
                        Case clsObjectLocation.StaticExistsWhereEnum.AllRooms
                            Return ALLROOMS
                        Case clsObjectLocation.StaticExistsWhereEnum.LocationGroup
                            Return .Key
                        Case clsObjectLocation.StaticExistsWhereEnum.NoRooms
                            Return HIDDEN
                        Case clsObjectLocation.StaticExistsWhereEnum.PartOfCharacter
                            Return Adventure.htblCharacters(.Key).BoundVisible
                        Case clsObjectLocation.StaticExistsWhereEnum.PartOfObject
                            Return Adventure.htblObjects(.Key).BoundVisible
                        Case clsObjectLocation.StaticExistsWhereEnum.SingleLocation
                            Return .Key
                    End Select
                Else
                    Select Case .DynamicExistWhere
                        Case clsObjectLocation.DynamicExistsWhereEnum.HeldByCharacter
                            Return Adventure.htblCharacters(.Key).BoundVisible
                        Case clsObjectLocation.DynamicExistsWhereEnum.Hidden
                            Return HIDDEN
                        Case clsObjectLocation.DynamicExistsWhereEnum.InLocation
                            Return .Key
                        Case clsObjectLocation.DynamicExistsWhereEnum.InObject
                            With Adventure.htblObjects(.Key)
                                If Not .Openable OrElse .IsOpen OrElse .IsTransparent Then
                                    Return Adventure.htblObjects(.Key).BoundVisible
                                Else
                                    Return .Key
                                End If
                            End With                            
                        Case clsObjectLocation.DynamicExistsWhereEnum.OnObject
                            Return Adventure.htblObjects(.Key).BoundVisible
                        Case clsObjectLocation.DynamicExistsWhereEnum.WornByCharacter
                            Return Adventure.htblCharacters(.Key).BoundVisible
                    End Select
                End If
            End With
            Return HIDDEN
        End Get
    End Property

    Public ReadOnly Property IsVisibleTo(ByVal sCharKey As String) As Boolean
        Get            
            If sCharKey = "%Player%" Then sCharKey = Adventure.Player.Key            

            Dim sBoundVisible As String = BoundVisible
            Dim sBoundVisibleCh As String = Adventure.htblCharacters(sCharKey).BoundVisible

            Select Case sBoundVisible
                Case HIDDEN
                    Return False
                Case ALLROOMS
                    Return Adventure.htblLocations.ContainsKey(sBoundVisibleCh)
            End Select


            If Adventure.htblGroups.ContainsKey(sBoundVisible) Then
                Return Adventure.htblGroups(sBoundVisible).arlMembers.Contains(sBoundVisibleCh)
            Else
                Return sBoundVisible = sBoundVisibleCh
            End If
        End Get
    End Property

    Public ReadOnly Property IsVisibleAtLocation(ByVal sLocationKey As String) As Boolean
        Get
            Dim sBoundVisible As String = BoundVisible

            Select Case sBoundVisible
                Case HIDDEN
                    Return False
                Case ALLROOMS
                    Return Adventure.htblLocations.ContainsKey(sLocationKey)
            End Select

            If Adventure.htblGroups.ContainsKey(sBoundVisible) Then
                Return Adventure.htblGroups(sBoundVisible).arlMembers.Contains(sLocationKey)
            Else
                Return sBoundVisible = sLocationKey
            End If
        End Get
    End Property

    Public Sub Move(ByVal ToWhere As clsObjectLocation)

        ' If we're moving into or onto an object, make sure we're not going recursive
        If ToWhere.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject OrElse ToWhere.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject Then
            If ToWhere.Key = Me.Key OrElse Adventure.htblObjects(Key).Children(WhereChildrenEnum.Everything).ContainsKey(ToWhere.Key) Then
                DisplayError("Can't move object " & Me.FullName & " " & If(ToWhere.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject, "inside", "onto").ToString & " " & Adventure.htblObjects(ToWhere.Key).FullName & " as that would create a recursive location.")
                Exit Sub
            End If
        End If

        sPrevParent = Parent
        Me.Location = ToWhere

        ' Update any 'seen' things  
        If Me.Key IsNot Nothing Then
            For Each Loc As clsLocation In LocationRoots.Values
                If Loc IsNot Nothing AndAlso IsVisibleAtLocation(Loc.Key) Then
                    For Each ch As clsCharacter In Adventure.htblLocations(Loc.Key).CharactersVisibleAtLocation.Values
                        ch.HasSeenObject(Me.Key) = True
                    Next
                End If
            Next
        End If
    End Sub


    Friend Enum WhereChildrenEnum
        InsideOrOnObject
        InsideObject
        OnObject
        Everything ' Includes objects that are part of this object
    End Enum

    Friend ReadOnly Property Children(ByVal WhereChildren As WhereChildrenEnum, Optional ByVal bRecursive As Boolean = False) As ObjectHashTable
        Get
            Dim htblChildren As New ObjectHashTable

            For Each ob As clsObject In Adventure.htblObjects.Values
                Dim bCheckSubObject As Boolean = False
                Select Case WhereChildren
                    Case WhereChildrenEnum.Everything
                        bCheckSubObject = (ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject OrElse ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject OrElse ob.Location.StaticExistWhere = clsObjectLocation.StaticExistsWhereEnum.PartOfObject)
                    Case WhereChildrenEnum.InsideOrOnObject
                        bCheckSubObject = (ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject OrElse ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject)
                    Case WhereChildrenEnum.InsideObject
                        bCheckSubObject = ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.InObject
                    Case WhereChildrenEnum.OnObject
                        bCheckSubObject = ob.Location.DynamicExistWhere = clsObjectLocation.DynamicExistsWhereEnum.OnObject
                End Select
                If bCheckSubObject Then
                    If ob.Location.Key = Me.Key Then
                        htblChildren.Add(ob, ob.Key)
                        If bRecursive Then
                            For Each obChild As clsObject In ob.Children(WhereChildrenEnum.Everything, True).Values
                                htblChildren.Add(obChild, obChild.Key)
                            Next
                        End If
                    End If
                End If
            Next
            If bRecursive Then
                ' Also got to check all characters inside the object to include anything held/worn by them
                For Each ch As clsCharacter In Adventure.htblCharacters.Values
                    If (WhereChildren <> WhereChildrenEnum.OnObject AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.InObject) OrElse (WhereChildren <> WhereChildrenEnum.InsideObject AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject) Then
                        If ch.Location.Key = Me.Key Then
                            For Each obHeldWornByChar As clsObject In ch.ChildrenObjects(WhereChildrenEnum.InsideOrOnObject, True).Values
                                htblChildren.Add(obHeldWornByChar, obHeldWornByChar.Key)
                            Next
                        End If
                    End If
                Next
            End If
            Return htblChildren
        End Get
    End Property


    Friend ReadOnly Property ChildrenCharacters(ByVal WhereChildren As WhereChildrenEnum) As CharacterHashTable
        Get
            Dim htblChildren As New CharacterHashTable

            For Each ch As clsCharacter In Adventure.htblCharacters.Values
                If (WhereChildren <> WhereChildrenEnum.OnObject AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.InObject) OrElse (WhereChildren <> WhereChildrenEnum.InsideObject AndAlso ch.Location.ExistWhere = clsCharacterLocation.ExistsWhereEnum.OnObject) Then
                    If ch.Location.Key = Me.Key Then
                        htblChildren.Add(ch, ch.Key)
                    End If
                End If
            Next

            Return htblChildren
        End Get
    End Property

    Public Sub New()
        oDescription = New Description
    End Sub

    Public Overrides ReadOnly Property Clone() As clsItem
        Get
            Dim ob As clsObject = CType(Me.MemberwiseClone, clsObject)
            ob.htblActualProperties = CType(ob.htblActualProperties.Clone, PropertyHashTable)
            ob.arlNames = ob.arlNames.Clone
            Return ob
        End Get
    End Property

    Public Overrides ReadOnly Property CommonName() As String
        Get
            Return FullName
        End Get
    End Property


    Friend Overrides ReadOnly Property AllDescriptions() As System.Collections.Generic.List(Of SharedModule.Description)
        Get
            Dim all As New Generic.List(Of Description)
            all.Add(Me.Description)
            For Each p As clsProperty In htblActualProperties.Values
                all.Add(p.StringData)
            Next
            Return all
        End Get
    End Property


    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object
        Dim iCount As Integer = iReplacements
        iReplacements += MyBase.FindStringInStringProperty(Me.sArticle, sSearchString, sReplace, bFindAll)
        iReplacements += MyBase.FindStringInStringProperty(Me.sPrefix, sSearchString, sReplace, bFindAll)
        For i As Integer = arlNames.Count - 1 To 0 Step -1
            iReplacements += MyBase.FindStringInStringProperty(Me.arlNames(i), sSearchString, sReplace, bFindAll)
        Next
        Return iReplacements - iCount
    End Function

    Protected Overrides ReadOnly Property PropertyGroupType() As clsGroup.GroupTypeEnum
        Get
            Return clsGroup.GroupTypeEnum.Objects
        End Get
    End Property

    Public Overrides Function ReferencesKey(ByVal sKey As String) As Integer

        Dim iCount As Integer = 0
        For Each d As Description In AllDescriptions
            iCount += d.ReferencesKey(sKey)
        Next        
        'If sKey = Location.Key Then iCount += 1        
        iCount += htblActualProperties.ReferencesKey(sKey)

        Return iCount

    End Function

    Public Overrides Function DeleteKey(ByVal sKey As String) As Boolean

        For Each d As Description In AllDescriptions
            d.DeleteKey(sKey)
        Next
        If Not htblActualProperties.DeleteKey(sKey) Then Return False
        ' TODO - Need to do something clever here.  E.g. if we have properties Where=InLocation then Location = theroom, we need to set Where=Hidden and remove Location property
        Return True
    End Function

End Class


Public Class clsObjectLocation

    Private _Key As String

    Public Enum DynamicExistsWhereEnum
        Hidden
        InLocation
        InObject
        OnObject
        HeldByCharacter
        WornByCharacter
    End Enum
    Public Property DynamicExistWhere As DynamicExistsWhereEnum

    Public Enum StaticExistsWhereEnum
        NoRooms
        SingleLocation
        LocationGroup
        AllRooms
        PartOfCharacter
        PartOfObject
    End Enum
    Public Property StaticExistWhere As StaticExistsWhereEnum

    Public Property Number As Integer

    Public Property Key() As String
        Get
            If _Key = "%Player%" AndAlso Adventure.Player IsNot Nothing Then _Key = Adventure.Player.Key
            Return _Key
        End Get
        Set(ByVal Value As String)
            If _Key IsNot Nothing AndAlso Value <> _Key Then
                _Key = _Key
            End If
            _Key = Value
        End Set
    End Property

    Public Function Copy() As clsObjectLocation
        Dim loc As New clsObjectLocation
        loc.Key = _Key
        loc.DynamicExistWhere = DynamicExistWhere
        loc.StaticExistWhere = StaticExistWhere
        loc.Number = Number
        Return loc
    End Function

End Class


