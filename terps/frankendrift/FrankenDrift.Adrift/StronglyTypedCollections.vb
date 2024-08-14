Imports System.Collections.Generic

Public Module StronglyTypedCollections
    ' TODO - phase out the individual hashtables in favour of a single dictionary

    Public Class ItemDictionary
        Private AllItems As Generic.Dictionary(Of String, clsItem)

        Public Sub New()
            AllItems = New Generic.Dictionary(Of String, clsItem)
        End Sub

        Public ReadOnly Property Values() As Generic.Dictionary(Of String, clsItem).ValueCollection
            Get
                Return AllItems.Values
            End Get
        End Property

        Public ReadOnly Property Keys() As Generic.Dictionary(Of String, clsItem).KeyCollection
            Get
                Return AllItems.Keys
            End Get
        End Property

        Public Function ContainsKey(ByVal key As String) As Boolean
            Return AllItems.ContainsKey(key)
        End Function

        Public Function TryGetValue(ByVal key As String, ByRef item As clsItem) As Boolean
            Return AllItems.TryGetValue(key, item)
        End Function

        Default Public Property Item(ByVal key As String) As clsItem
            Get
                Dim itm As clsItem = Nothing
                If TryGetValue(key, itm) Then Return itm
                Return Nothing
            End Get
            Set(ByVal value As clsItem)
                AllItems(key) = value
            End Set
        End Property

        Public Sub AddBase(ByVal item As clsItem)
            AllItems.Add(item.Key, item)
        End Sub

        Public Sub RemoveBase(ByVal key As String)
            AllItems.Remove(key)
        End Sub

        ' This is currently just a view of the existing hashtables

        Shadows Sub Add(ByVal item As clsItem)
            Select Case True
                Case TypeOf item Is clsLocation
                    Adventure.htblLocations.Add(CType(item, clsLocation), item.Key)
                Case TypeOf item Is clsObject
                    Adventure.htblObjects.Add(CType(item, clsObject), item.Key)
                Case TypeOf item Is clsTask
                    Adventure.htblTasks.Add(CType(item, clsTask), item.Key)
                Case TypeOf item Is clsEvent
                    Adventure.htblEvents.Add(CType(item, clsEvent), item.Key)
                Case TypeOf item Is clsCharacter
                    Adventure.htblCharacters.Add(CType(item, clsCharacter), item.Key)
                Case TypeOf item Is clsVariable
                    Adventure.htblVariables.Add(CType(item, clsVariable), item.Key)
                Case TypeOf item Is clsALR
                    Adventure.htblALRs.Add(CType(item, clsALR), item.Key)
                Case TypeOf item Is clsGroup
                    Adventure.htblGroups.Add(CType(item, clsGroup), item.Key)
                Case TypeOf item Is clsHint
                    Adventure.htblHints.Add(CType(item, clsHint), item.Key)
                Case TypeOf item Is clsProperty
                    Adventure.htblAllProperties.Add(CType(item, clsProperty))
                Case TypeOf item Is clsUserFunction
                    Adventure.htblUDFs.Add(CType(item, clsUserFunction), item.Key)
                Case Else
                    TODO("Add item of type " & item.ToString)
            End Select
        End Sub


        Shadows Sub Remove(ByVal key As String)
            If Adventure.htblLocations.ContainsKey(key) Then
                Adventure.htblLocations.Remove(key)
            ElseIf Adventure.htblObjects.ContainsKey(key) Then
                Adventure.htblObjects.Remove(key)
            ElseIf Adventure.htblTasks.ContainsKey(key) Then
                Adventure.htblTasks.Remove(key)
            ElseIf Adventure.htblEvents.ContainsKey(key) Then
                Adventure.htblEvents.Remove(key)
            ElseIf Adventure.htblCharacters.ContainsKey(key) Then
                Adventure.htblCharacters.Remove(key)
            ElseIf Adventure.htblVariables.ContainsKey(key) Then
                Adventure.htblVariables.Remove(key)
            ElseIf Adventure.htblALRs.ContainsKey(key) Then
                Adventure.htblALRs.Remove(key)
            ElseIf Adventure.htblAllProperties.ContainsKey(key) Then
                Adventure.htblAllProperties.Remove(key)
            ElseIf Adventure.htblHints.ContainsKey(key) Then
                Adventure.htblHints.Remove(key)
            ElseIf Adventure.htblGroups.ContainsKey(key) Then
                Adventure.htblGroups.Remove(key)
            ElseIf Adventure.htblUDFs.ContainsKey(key) Then
                Adventure.htblUDFs.Remove(key)
            Else
                TODO("Remove item of type " & key)
            End If
        End Sub

    End Class

    Public Class LocationHashTable
        Inherits Dictionary(Of String, clsLocation)

        Shadows Sub Add(ByVal loc As clsLocation, ByVal key As String)
            MyBase.Add(key, loc)
            If Me Is Adventure.htblLocations Then Adventure.dictAllItems.AddBase(loc)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            If Me Is Adventure.htblLocations Then Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsLocation
            Get
                'Return CType(MyBase.Item(key), clsLocation)
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsLocation)
                MyBase.Item(key) = Value
            End Set
        End Property

        Public Function List(Optional ByVal sSeparator As String = "and") As String
            Dim iCount As Integer = MyBase.Count

            List = Nothing

            For Each loc As clsLocation In MyBase.Values
                List &= loc.ShortDescription.ToString(True)
                iCount -= 1
                If iCount > 1 Then List &= ", "
                If iCount = 1 Then List &= " " & sSeparator & " "
            Next
            If List = "" Then List = "nowhere"

        End Function
    End Class

    Public Class ObjectHashTable
        Inherits Dictionary(Of String, clsObject)

        Shadows Sub Add(ByVal ob As clsObject, ByVal key As String)
            MyBase.Add(key, ob)
            If Me Is Adventure.htblObjects Then Adventure.dictAllItems.AddBase(ob)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            If Me Is Adventure.htblObjects Then Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsObject
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsObject)
                MyBase.Item(key) = Value
            End Set
        End Property

        Public Function List(Optional ByVal sSeparator As String = "and", Optional ByVal bIncludeSubObjects As Boolean = False, Optional ByVal Article As ArticleTypeEnum = ArticleTypeEnum.Definite) As String
            Dim iCount As Integer = MyBase.Count

            List = Nothing

            For Each ob As clsObject In MyBase.Values
                List &= ob.FullName(Article)
                iCount -= 1
                If iCount > 1 Then List &= ", "
                If iCount = 1 Then List &= " " & sSeparator & " "
            Next
            If List = "" Then List = "nothing"

            If bIncludeSubObjects Then
                For Each ob As clsObject In MyBase.Values

                    If ob.Children(clsObject.WhereChildrenEnum.OnObject).Count > 0 Then
                        If List <> "" Then List &= ".  "
                        List &= ToProper(ob.Children(clsObject.WhereChildrenEnum.OnObject).List("and", False, Article))
                        If ob.Children(clsObject.WhereChildrenEnum.OnObject).Count = 1 Then
                            List &= " is on "
                        Else
                            List &= " are on "
                        End If
                        List &= ob.FullName(ArticleTypeEnum.Definite)
                    End If
                    If (ob.Openable AndAlso ob.IsOpen) OrElse Not ob.Openable Then
                        If ob.Children(clsObject.WhereChildrenEnum.InsideObject).Count > 0 Then
                            If ob.Children(clsObject.WhereChildrenEnum.OnObject).Count > 0 Then
                                List &= ", and inside"
                            Else
                                If List <> "" Then List &= ".  "
                                List &= "Inside " & ob.FullName(ArticleTypeEnum.Definite)
                            End If
                            If ob.Children(clsObject.WhereChildrenEnum.InsideObject).Count = 1 Then
                                List &= " is "
                            Else
                                List &= " are "
                            End If
                            List &= ob.Children(clsObject.WhereChildrenEnum.InsideObject).List("and", False, Article)
                        End If

                    End If
                Next
            End If
        End Function

        Public Function SeenBy(Optional ByVal sCharKey As String = "") As ObjectHashTable

            If sCharKey = "" OrElse sCharKey = "%Player%" Then sCharKey = Adventure.Player.Key

            Dim htbl As New ObjectHashTable

            For Each ob As clsObject In Me.Values
                If ob.SeenBy(sCharKey) Then htbl.Add(ob, ob.Key)
            Next

            Return htbl

        End Function

        Public Function VisibleTo(Optional ByVal sCharKey As String = "") As ObjectHashTable

            If sCharKey = "" OrElse sCharKey = "%Player%" Then sCharKey = Adventure.Player.Key

            Dim htbl As New ObjectHashTable

            For Each ob As clsObject In Me.Values
                If ob.IsVisibleTo(sCharKey) Then htbl.Add(ob, ob.Key)
            Next

            Return htbl

        End Function

    End Class


    Public Class TaskHashTable
        Inherits Dictionary(Of String, clsTask)

        Shadows Sub Add(ByVal task As clsTask, ByVal key As String)
            If MyBase.ContainsKey(key) Then
                key = key
            End If
            MyBase.Add(key, task)
            If Me Is Adventure.htblTasks Then Adventure.dictAllItems.AddBase(task)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            If Me Is Adventure.htblTasks Then Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsTask
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsTask)
                MyBase.Item(key) = Value
            End Set
        End Property
    End Class

    ' Exposes an array of keys that we can iterate that are ordered in our original way
    Public Class OrderedHashTable
        Inherits Hashtable

        Public OrderedKeys As New StringArrayList


        Shadows Sub Add(ByVal key As String, ByVal value As Object)
            MyBase.Add(key, value)
            OrderedKeys.Add(key)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            OrderedKeys.Remove(key)
        End Sub

        Shadows Sub Clear()
            MyBase.Clear()
            OrderedKeys.Clear()
        End Sub

        Public Sub Insert(ByVal key As String, ByVal value As Object, ByVal iPosition As Integer)
            MyBase.Add(key, value)
            OrderedKeys.Insert(iPosition, key)
        End Sub

        Public Shadows Function Clone() As OrderedHashTable
            Dim htbl As New OrderedHashTable

            For Each sKey As String In OrderedKeys
                htbl.Add(sKey, Item(sKey))
            Next

            Return htbl
        End Function

    End Class

    Public Class StringHashTable
        Inherits Dictionary(Of String, String)

        Default Shadows Property Item(ByVal key As String) As String
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As String)
                MyBase.Item(key) = Value
            End Set
        End Property

        Shadows Function Clone() As StringHashTable
            Dim htblReturn As New StringHashTable

            For Each entry As KeyValuePair(Of String, String) In Me
                htblReturn.Add(entry.Key, entry.Value)
            Next

            Return htblReturn
        End Function

    End Class


    Public Class EventHashTable
        Inherits Dictionary(Of String, clsEvent)

        Shadows Sub Add(ByVal evnt As clsEvent, ByVal key As String)
            MyBase.Add(key, evnt)
            Adventure.dictAllItems.AddBase(evnt)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsEvent
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsEvent)
                MyBase.Item(key) = Value
            End Set
        End Property

    End Class


    Public Class CharacterHashTable
        Inherits Dictionary(Of String, clsCharacter)

        Shadows Sub Add(ByVal chrr As clsCharacter, ByVal key As String)
            MyBase.Add(key, chrr)
            If Me Is Adventure.htblCharacters Then Adventure.dictAllItems.AddBase(chrr)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            If Me Is Adventure.htblCharacters Then Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsCharacter
            Get
                If key = "%Player%" Then
                    If Adventure.Player Is Nothing Then Return Nothing
                    key = Adventure.Player.Key
                End If
                Return MyBase.Item(key)
            End Get
            Set(ByVal Value As clsCharacter)
                MyBase.Item(key) = Value
            End Set
        End Property

        Public Function List(Optional ByVal sSeparator As String = "and") As String
            Dim iCount As Integer = MyBase.Count

            List = Nothing

            For Each ch As clsCharacter In MyBase.Values
                List &= "%CharacterName[" & ch.Key & "]%"
                iCount -= 1
                If iCount > 1 Then List &= ", "
                If iCount = 1 Then List &= " " & sSeparator & " "
            Next
            If List = "" Then List = "noone"
        End Function

        Public Function SeenBy(Optional ByVal sCharKey As String = "") As CharacterHashTable
            If sCharKey = "" OrElse sCharKey = "%Player%" Then sCharKey = Adventure.Player.Key

            Dim htbl As New CharacterHashTable

            For Each ch As clsCharacter In Me.Values
                If ch.SeenBy(sCharKey) Then htbl.Add(ch, ch.Key)
            Next
            Return htbl
        End Function

        Public Function VisibleTo(Optional ByVal sCharKey As String = "") As CharacterHashTable
            If sCharKey = "" OrElse sCharKey = "%Player%" Then sCharKey = Adventure.Player.Key

            Dim htbl As New CharacterHashTable

            For Each ch As clsCharacter In Me.Values
                If ch.CanSeeCharacter(sCharKey) Then htbl.Add(ch, ch.Key)
            Next

            Return htbl
        End Function

    End Class


    Public Class GroupHashTable
        Inherits Dictionary(Of String, clsGroup)

        Shadows Sub Add(ByVal grp As clsGroup, ByVal key As String)
            MyBase.Add(key, grp)
            Adventure.dictAllItems.AddBase(grp)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsGroup
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsGroup)
                MyBase.Item(key) = Value
            End Set
        End Property
    End Class


    Public Class ALRHashTable
        Inherits Dictionary(Of String, clsALR)

        Shadows Sub Add(ByVal alr As clsALR, ByVal key As String)
            MyBase.Add(key, alr)
            Adventure.dictAllItems.AddBase(alr)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsALR
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsALR)
                MyBase.Item(key) = Value
            End Set
        End Property
    End Class


    Public Class UDFHashTable
        Inherits Dictionary(Of String, clsUserFunction)

        Shadows Sub Add(ByVal UDF As clsUserFunction, ByVal key As String)
            MyBase.Add(key, UDF)
            Adventure.dictAllItems.AddBase(UDF)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsUserFunction
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsUserFunction)
                MyBase.Item(key) = Value
            End Set
        End Property
    End Class


    Public Class VariableHashTable
        Inherits Dictionary(Of String, clsVariable)

        Shadows Sub Add(ByVal var As clsVariable, ByVal key As String)
            MyBase.Add(key, var)
            Adventure.dictAllItems.AddBase(var)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsVariable
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsVariable)
                MyBase.Item(key) = Value
            End Set
        End Property
    End Class


    Public Class SynonymHashTable
        Inherits Dictionary(Of String, clsSynonym)

        Shadows Sub Add(ByVal syn As clsSynonym)
            MyBase.Add(syn.Key, syn)
            Adventure.dictAllItems.AddBase(syn)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsSynonym
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsSynonym)
                MyBase.Item(key) = Value
            End Set
        End Property
    End Class



    Public Class RestrictionArrayList
        Inherits List(Of clsRestriction)
        Implements ICloneable

        Private sBracketSequence As String

        Public Property BracketSequence() As String
            Get
                Return sBracketSequence
            End Get
            Set(ByVal Value As String)
                If sBracketSequence <> Value Then
                    sBracketSequence = Value
                End If
            End Set
        End Property


        Public Function ReferencesKey(ByVal sKey As String) As Integer
            Dim iCount As Integer = 0
            For Each r As clsRestriction In Me
                If r.ReferencesKey(sKey) Then iCount += 1
            Next
            Return iCount
        End Function


        Public Function DeleteKey(ByVal sKey As String) As Boolean
            For i As Integer = MyBase.Count - 1 To 0 Step -1
                If CType(MyBase.Item(i), clsRestriction).ReferencesKey(sKey) Then
                    RemoveAt(i)
                    If MyBase.Count = 0 Then
                        BracketSequence = ""
                    ElseIf MyBase.Count = 1 Then
                        BracketSequence = "#"
                    Else
                        StripRestriction(i)
                    End If
                End If
            Next

            Return True
        End Function


        Private Function StripRestriction(ByVal iRest As Integer) As Boolean
            Dim iFound As Integer = 0

            For i As Integer = 0 To BracketSequence.Length - 1
                If sMid(BracketSequence, i, 1) = "#" Then iFound += 1
                If iFound = iRest + 1 Then
                    ' Delete the marker
                    BracketSequence = sLeft(BracketSequence, i) & sRight(BracketSequence, BracketSequence.Length - i - 1)
                    ' Remove any trailing And/Or markers
                    If BracketSequence.Length >= i AndAlso (sMid(BracketSequence, i, 1) = "A" OrElse sMid(BracketSequence, i, 1) = "O") Then
                        BracketSequence = sLeft(BracketSequence, i) & sRight(BracketSequence, BracketSequence.Length - i - 1)
                    End If
                    ' Correct any duff brackets
                    Exit For
                End If
            Next
        End Function


        Public Function IsBracketsValid() As Boolean
            If sBracketSequence Is Nothing Then Return True
            Dim sBS As String = sBracketSequence.Replace("[", "((").Replace("]", "))")
            Dim sTemp As String = Nothing

            While sTemp <> sBS
                sTemp = sBS
                sBS = Replace(sBS, "exprAexpr", "expr")
                sBS = Replace(sBS, "exprOexpr", "expr")
                sBS = Replace(sBS, "#", "expr")
                sBS = Replace(sBS, "(expr)", "expr")
                sBS = Replace(sBS, "[expr]", "expr")
                'Debug.Print(brackstring)
            End While

            If sBS = "expr" OrElse (Me.Count = 0 AndAlso sBS = "") Then
                Return True
            Else
                Return False
            End If
        End Function

        Default Shadows Property Item(ByVal idx As Integer) As clsRestriction
            Get
                Try
                    Return MyBase.Item(idx)
                Catch ex As Exception
                    Return Nothing
                End Try
                'Return CType(MyBase.Item(idx), clsRestriction)
            End Get
            Set(ByVal Value As clsRestriction)
                MyBase.Item(idx) = Value
            End Set
        End Property

        Public Function Copy() As RestrictionArrayList
            Dim ral As New RestrictionArrayList

            ral.BracketSequence = sBracketSequence
            For i As Integer = 0 To MyBase.Count - 1
                Dim rest As clsRestriction = CType(MyBase.Item(i), clsRestriction).Copy
                ral.Add(rest)
            Next

            Return ral
        End Function

        Private Function Clone() As Object Implements ICloneable.Clone
            Return Me.MemberwiseClone
        End Function
        Public Function CloneMe() As RestrictionArrayList
            Return CType(Clone(), RestrictionArrayList)
        End Function
    End Class



    Public Class ActionArrayList
        Inherits List(Of clsAction)

        Public Function ReferencesKey(ByVal sKey As String) As Integer
            Dim iCount As Integer = 0
            For Each a As clsAction In Me
                If a.ReferencesKey(sKey) Then iCount += 1
            Next
            Return iCount
        End Function

        Public Function DeleteKey(ByVal sKey As String) As Boolean
            For i As Integer = MyBase.Count - 1 To 0 Step -1
                If CType(MyBase.Item(i), clsAction).ReferencesKey(sKey) Then
                    RemoveAt(i)
                End If
            Next
            Return True
        End Function

        Default Shadows Property Item(ByVal idx As Integer) As clsAction
            Get
                Try
                    Return MyBase.Item(idx)
                Catch ex As Exception
                    Return Nothing
                End Try
                'Return CType(MyBase.Item(idx), clsAction)
            End Get
            Set(ByVal Value As clsAction)
                MyBase.Item(idx) = Value
            End Set
        End Property

        Public Function Copy() As ActionArrayList
            Dim aal As New ActionArrayList

            For i As Integer = 0 To MyBase.Count - 1
                Dim act As clsAction = CType(MyBase.Item(i), clsAction).Copy
                aal.Add(act)
            Next

            Return aal
        End Function

    End Class


    Public Class EventDescriptionArrayList
        Inherits List(Of clsEventDescription)

        Default Shadows Property Item(ByVal idx As Integer) As clsEventDescription
            Get
                Try
                    Return MyBase.Item(idx)
                Catch ex As Exception
                    Return Nothing
                End Try
                'Return CType(MyBase.Item(idx), clsEventDescription)
            End Get
            Set(ByVal Value As clsEventDescription)
                MyBase.Item(idx) = Value
            End Set
        End Property
    End Class


    Public Class PropertyHashTable
        Inherits Dictionary(Of String, clsProperty)

        Shadows Sub Add(ByVal prop As clsProperty)

            MyBase.Add(prop.Key, prop)
            If Me Is Adventure.htblAllProperties Then
                Select Case prop.PropertyOf
                    Case clsProperty.PropertyOfEnum.Objects
                        Adventure.htblObjectProperties.Add(prop)
                    Case clsProperty.PropertyOfEnum.Locations
                        Adventure.htblLocationProperties.Add(prop)
                    Case clsProperty.PropertyOfEnum.Characters
                        Adventure.htblCharacterProperties.Add(prop)
                    Case clsProperty.PropertyOfEnum.AnyItem
                        Adventure.htblObjectProperties.Add(prop)
                        Adventure.htblLocationProperties.Add(prop)
                        Adventure.htblCharacterProperties.Add(prop)
                End Select
                Adventure.dictAllItems.AddBase(prop)
            End If
        End Sub

        Shadows Sub Remove(ByVal key As String)
            If Me Is Adventure.htblAllProperties Then
                Select Case Adventure.htblAllProperties(key).PropertyOf
                    Case clsProperty.PropertyOfEnum.Objects
                        Adventure.htblObjectProperties.Remove(key)
                    Case clsProperty.PropertyOfEnum.Characters
                        Adventure.htblCharacterProperties.Remove(key)
                    Case clsProperty.PropertyOfEnum.Locations
                        Adventure.htblLocationProperties.Remove(key)
                    Case clsProperty.PropertyOfEnum.AnyItem
                        Adventure.htblObjectProperties.Remove(key)
                        Adventure.htblCharacterProperties.Remove(key)
                        Adventure.htblLocationProperties.Remove(key)
                End Select
                Adventure.dictAllItems.RemoveBase(key)
            End If
            MyBase.Remove(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsProperty
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsProperty)
                MyBase.Item(key) = Value
            End Set
        End Property

        Public Function CopySelected(Optional ByVal bGroup As Boolean = False) As PropertyHashTable

            Dim pht As New PropertyHashTable

            For Each prop As clsProperty In MyBase.Values
                If prop.Selected AndAlso (bGroup OrElse Not (prop.GroupOnly OrElse prop.FromGroup)) Then pht.Add(prop.Copy)
            Next

            Return pht

        End Function

        Shadows Function Clone() As PropertyHashTable
            Dim htblReturn As New PropertyHashTable

            For Each entry As KeyValuePair(Of String, clsProperty) In Me
                htblReturn.Add(CType(entry.Value.Clone, clsProperty))
            Next

            Return htblReturn
        End Function


        Public Function ReferencesKey(ByVal sKey As String) As Integer
            Dim iCount As Integer = 0
            If MyBase.ContainsKey(sKey) Then iCount += 1
            For Each p As clsProperty In MyBase.Values
                If p.AppendToProperty = sKey Then iCount += 1
                If p.DependentKey = sKey Then iCount += 1
                If p.RestrictProperty = sKey Then iCount += 1
                If p.Type = clsProperty.PropertyTypeEnum.CharacterKey OrElse p.Type = clsProperty.PropertyTypeEnum.LocationGroupKey OrElse p.Type = clsProperty.PropertyTypeEnum.LocationKey OrElse p.Type = clsProperty.PropertyTypeEnum.ObjectKey Then
                    If p.Value = sKey Then iCount += 1
                End If
            Next
            Return iCount
        End Function


        ''' <summary>
        ''' 
        ''' </summary>
        ''' <param name="p">Property to reset or remove</param>
        ''' <returns>True, if we remove a property</returns>
        ''' <remarks></remarks>
        Private Function ResetOrRemoveProperty(ByVal p As clsProperty) As Boolean
            ' If we're not mandatory we can just remove the property
            If Not p.Mandatory Then
                ' Ok, just remove it
                Remove(p.Key)
                Return True
            Else
                ' If we are mandatory, we may be able to reset the value, depending on the property type
                Dim bDelete As Boolean = False
                Select Case p.Type
                    Case clsProperty.PropertyTypeEnum.CharacterKey, clsProperty.PropertyTypeEnum.LocationGroupKey, clsProperty.PropertyTypeEnum.LocationKey, clsProperty.PropertyTypeEnum.ObjectKey
                        ' Key types can't be reset, must be deleted, as they can't be Null on a mandatory property
                        bDelete = True
                    Case clsProperty.PropertyTypeEnum.Integer
                        p = p
                    Case clsProperty.PropertyTypeEnum.Text
                        p = p
                    Case clsProperty.PropertyTypeEnum.StateList
                        ' Ok, we just need to make sure we change the value to something that doesn't produce child property                        
                        Dim bAssignedValue As Boolean = False
                        For Each sValue As String In p.arlStates
                            Dim bSafeValue As Boolean = True
                            ' If we're unable to do this, we need to remove, and reset/remove parent
                            For Each pOther As clsProperty In Me.Values
                                If pOther.DependentKey = p.Key AndAlso pOther.DependentValue = sValue Then
                                    bSafeValue = False
                                    Exit For
                                End If
                            Next
                            If bSafeValue Then
                                ' Ok to set the value to this
                                p.Value = sValue
                                bAssignedValue = True
                                Exit For
                            End If
                        Next
                        If Not bAssignedValue Then bDelete = True
                End Select

                ' If we can't reset the property, we need to reset or remove our parent
                If bDelete Then
                    If ContainsKey(p.DependentKey) Then
                        Dim pParent As clsProperty = Me.Item(p.DependentKey)
                        ResetOrRemoveProperty(pParent)
                    End If
                    Remove(p.Key)
                    Return True
                End If
            End If

            Return False
        End Function


        Public Function DeleteKey(ByVal sKey As String) As Boolean
restart:
            For Each p As clsProperty In MyBase.Values
                If p.AppendToProperty = sKey Then p.AppendToProperty = ""
                If p.DependentKey = sKey Then p.DependentKey = ""
                If p.RestrictProperty = sKey Then p.RestrictProperty = ""
                If p.Type = clsProperty.PropertyTypeEnum.CharacterKey OrElse p.Type = clsProperty.PropertyTypeEnum.LocationGroupKey OrElse p.Type = clsProperty.PropertyTypeEnum.LocationKey OrElse p.Type = clsProperty.PropertyTypeEnum.ObjectKey Then
                    If p.Value = sKey Then
                        If ResetOrRemoveProperty(p) Then GoTo restart
                    End If
                End If
            Next
            If ContainsKey(sKey) Then Remove(sKey)
            Return True
        End Function

        ' Ensures any child properties have their Selected properties set
        Public Sub SetSelected()
            For Each prop As clsProperty In MyBase.Values
                prop.Selected = IsPropertySelected(prop)
            Next
        End Sub

        Private Function IsPropertySelected(ByVal prop As clsProperty) As Boolean
            If SafeString(prop.DependentKey) <> "" Then
                If MyBase.ContainsKey(prop.DependentKey) Then
                    If prop.DependentValue Is Nothing OrElse Item(prop.DependentKey).Value = prop.DependentValue Then
                        Return IsPropertySelected(Item(prop.DependentKey))
                    End If
                Else
                    WarnOnce($"Property ""{prop.Description}"" is dependent on ""{prop.DependentKey}"" but this property doesn't exist")
                End If
            Else
                Return prop.Selected
            End If
        End Function

    End Class


    Public Class BooleanHashTable
        Inherits Dictionary(Of String, Boolean)

        Shadows Sub Add(ByVal bool As Boolean, ByVal key As String)
            MyBase.Add(key, bool)
        End Sub

        Default Shadows Property Item(ByVal key As String) As Boolean
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As Boolean)
                MyBase.Item(key) = Value
            End Set
        End Property

    End Class


    Public Class HintHashTable
        Inherits Dictionary(Of String, clsHint)

        Shadows Sub Add(ByVal hint As clsHint, ByVal key As String)
            MyBase.Add(key, hint)
            Adventure.dictAllItems.AddBase(hint)
        End Sub

        Shadows Sub Remove(ByVal key As String)
            MyBase.Remove(key)
            Adventure.dictAllItems.RemoveBase(key)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsHint
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsHint)
                MyBase.Item(key) = Value
            End Set
        End Property
    End Class


    Public Class TopicHashTable
        Inherits Dictionary(Of String, clsTopic)

        Shadows Sub Add(ByVal topic As clsTopic)
            MyBase.Add(topic.Key, topic)
        End Sub

        Default Shadows Property Item(ByVal key As String) As clsTopic
            Get
                Try
                    Return MyBase.Item(key)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsTopic)
                MyBase.Item(key) = Value
            End Set
        End Property

        Shadows Function Clone() As TopicHashTable
            Dim htblReturn As New TopicHashTable

            For Each entry As KeyValuePair(Of String, clsTopic) In Me
                htblReturn.Add(entry.Value.Clone)
            Next

            Return htblReturn
        End Function

        Friend Function DoesTopicHaveChildren(ByVal key As String) As Boolean
            For Each sKey As String In Me.Keys
                If key <> sKey Then
                    If Me.Item(sKey).ParentKey = key Then Return True
                End If
            Next
            Return False
        End Function

    End Class


    Public Class StringArrayList
        Inherits List(Of String)

        Default Shadows Property Item(ByVal idx As Integer) As String
            Get
                Try
                    Return MyBase.Item(idx)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As String)
                Try
                    MyBase.Item(idx) = Value
                Catch ex As Exception
                    ErrMsg("Error assigning value """ & Value & """ to index " & idx)
                End Try
            End Set
        End Property

        Sub Merge(ByVal sal As StringArrayList)
            For Each s As String In sal
                MyBase.Add(s)
            Next
        End Sub

        Shadows Function Clone() As StringArrayList
            Dim htblReturn As New StringArrayList

            For Each entry As String In Me
                htblReturn.Add(entry)
            Next

            Return htblReturn
        End Function

        Shadows Function Contains(ByVal strg As String) As Boolean
            Return MyBase.Contains(strg)
        End Function

        Function List() As String
            Dim iCount As Integer = MyBase.Count

            List = Nothing

            For Each s As String In Me
                List &= s
                iCount -= 1
                If iCount > 1 Then List &= ", "
                If iCount = 1 Then List &= " and "
            Next
            If List = "" Then List = "nothing"
        End Function

    End Class


    Public Class DirectionArrayList
        Inherits List(Of clsDirection)

        Default Shadows Property Item(ByVal idx As DirectionsEnum) As clsDirection
            Get
                Try
                    Return MyBase.Item(idx)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsDirection)
                MyBase.Item(idx) = Value
            End Set
        End Property

    End Class



    Public Class WalkArrayList
        Inherits List(Of clsWalk)

        Default Shadows Property Item(ByVal idx As Integer) As clsWalk
            Get
                Try
                    Return MyBase.Item(idx)
                Catch ex As Exception
                    Return Nothing
                End Try
            End Get
            Set(ByVal Value As clsWalk)
                MyBase.Item(idx) = Value
            End Set
        End Property

    End Class

End Module
