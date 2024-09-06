#If Adravalon Then
Imports FrankenDrift.Glue.Util
#End If

Public MustInherit Class clsItem

    Public Property Key() As String      
    Public Property IsLibrary() As Boolean
    Public Property LoadedFromLibrary As Boolean ' To distinguish library items loaded from the game - we want to prompt to overwrite library items in games, but not library items loaded from a library
    Public Property Tag As Object

    Private ReadOnly Property GenerateKey As String
        Get
            Dim sKey As String = Me.CommonName
            Try
                If sKey = "New Folder" Then sKey = "Folder"
                If TypeOf Me Is clsObject AndAlso CType(Me, clsObject).arlNames.Count > 0 Then sKey = CType(Me, clsObject).arlNames(0)

                Dim sNewKey As New System.Text.StringBuilder
                Dim bCaps As Boolean = True
                Dim AllKeys As List(Of String) = Adventure.AllKeys

                If sKey Is Nothing Then sKey = ""
                For iPos As Integer = 0 To sKey.Length - 1
                    If Char.IsLetterOrDigit(sKey(iPos)) Then
                        sNewKey.Append(If(bCaps, Char.ToUpper(sKey(iPos)), Char.ToLower(sKey(iPos))).ToString)
                        bCaps = False
                    Else
                        bCaps = True
                    End If
                Next

                sKey = sLeft(sNewKey.ToString, 10)
                If sKey.Length < 2 Then
                    sKey = GetNewKey(True)
                End If
                If AllKeys.FindAll(Function(s) s.IndexOf(sKey, StringComparison.OrdinalIgnoreCase) >= 0).Count = 0 Then Return sKey ' Case insensitive Keys.Contains

                Dim i As Integer = 1
                Do
                    If AllKeys.FindAll(Function(s) s.IndexOf(sKey & i, StringComparison.OrdinalIgnoreCase) >= 0).Count = 0 Then Return sKey & i
                    i += 1
                Loop
            Finally
                If Adventure.listExcludedItems.Contains(sKey) Then Adventure.listExcludedItems.Remove(sKey)
            End Try
        End Get
    End Property


    Public Function GetNewKey(Optional ByVal bForceOldStyle As Boolean = False) As String

        If Not bForceOldStyle AndAlso SafeBool(CInt(GetSetting("ADRIFT", "Generator", "KeyNames", "-1"))) Then
            Return GenerateKey
        Else
            Dim sType As String = ""
            Select Case True
                Case TypeOf Me Is clsLocation
                    sType = "Location"
                Case TypeOf Me Is clsObject
                    sType = "Object"
                Case TypeOf Me Is clsTask
                    sType = "Task"
                Case TypeOf Me Is clsEvent
                    sType = "Event"
                Case TypeOf Me Is clsCharacter
                    sType = "Character"
                Case TypeOf Me Is clsVariable
                    sType = "Variable"
                Case TypeOf Me Is clsALR
                    sType = "ALR"
                Case TypeOf Me Is clsGroup
                    sType = "Group"
                Case TypeOf Me Is clsHint
                    sType = "Hint"
                Case TypeOf Me Is clsProperty
                    sType = "Property"
                Case TypeOf Me Is clsSynonym
                    sType = "Synonym"
                Case TypeOf Me Is clsUserFunction
                    sType = "User Function"
                Case Else
                    TODO("Other types")
            End Select

            If sType <> "" Then
                Key = GetNewKey(sType)
                Return Key
            Else
                Return Nothing
            End If
        End If

    End Function


    Public Function GetNewKey(ByVal sPrefix As String) As String
        Dim iNum As Integer = 1

        While Not Adventure.GetTypeFromKey(sPrefix & iNum) Is Nothing
            iNum += 1
        End While

        Dim sKey As String = sPrefix & iNum

        If Adventure.listExcludedItems.Contains(sKey) Then Adventure.listExcludedItems.Remove(sKey)

        Return sKey
    End Function


    Private dtCreated As Date = Date.MinValue
    Friend Property Created() As Date
        Get            
            If dtCreated = Date.MinValue Then
                If dtLastUpdated < Now AndAlso dtLastUpdated > Date.MinValue Then
                    dtCreated = dtLastUpdated
                Else
                    dtCreated = Now
                End If
            End If
            Return dtCreated
        End Get
        Set(ByVal value As Date)
            dtCreated = value
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


    Public MustOverride ReadOnly Property CommonName() As String
    Public MustOverride ReadOnly Property Clone() As clsItem
    Friend MustOverride ReadOnly Property AllDescriptions() As Generic.List(Of Description)

    Public MustOverride Function ReferencesKey(ByVal sKey As String) As Integer
    Public MustOverride Function DeleteKey(ByVal sKey As String) As Boolean
    Friend MustOverride Function FindStringLocal(ByVal sSearchString As String, Optional ByVal sReplace As String = Nothing, Optional ByVal bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object


    Public Function SearchFor(ByVal sSearchString As String) As Boolean
        Dim oSearch As Object = FindString(sSearchString, , False)
        If TypeOf oSearch Is SingleDescription Then
            Return True
        Else
            Return CInt(oSearch) > 0
        End If
    End Function


    Public Function FindString(ByVal sSearchString As String, Optional ByVal sReplace As String = Nothing, Optional ByVal bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object

        Dim sCommonName As String = Me.CommonName
        Try
            For Each desc As Description In AllDescriptions
                Dim o As Object = FindStringInDescription(desc, sSearchString, sReplace, bFindAll, iReplacements)
                If o IsNot Nothing AndAlso Not bFindAll Then Return o
            Next
            Return FindStringLocal(sSearchString, sReplace, bFindAll, iReplacements)
        Catch ex As Exception
            ErrMsg("Error finding string " & sSearchString & " in item " & CommonName, ex)
        End Try

        Return Nothing
    End Function


    Private Function FindText(ByVal sTextToSearch As String, ByVal sTextToFind As String) As Boolean
        If sTextToSearch Is Nothing Then Return False

        Dim re As System.Text.RegularExpressions.Regex
        Dim sWordBound As String = CStr(IIf(SearchOptions.bFindExactWord, "\b", ""))
        sTextToFind = sTextToFind.Replace("\", "\\").Replace("*", ".*").Replace("?", "\?").Replace("[", "\[").Replace("]", "\]").Replace("(", "\(").Replace(")", "\)").Replace(".", "\.")
        If SearchOptions.bSearchMatchCase Then
            re = New System.Text.RegularExpressions.Regex(sWordBound & sTextToFind & sWordBound)
        Else
            re = New System.Text.RegularExpressions.Regex(sWordBound & sTextToFind & sWordBound, System.Text.RegularExpressions.RegexOptions.IgnoreCase)
        End If

        Return re.IsMatch(sTextToSearch)
    End Function


    Friend Function FindStringInStringProperty(ByRef text As String, ByVal sSearchString As String, Optional ByVal sReplace As String = Nothing, Optional ByVal bFindAll As Boolean = True) As Integer
        If FindText(text, sSearchString) Then
            Dim iReplacements As Integer = 0
            If sReplace IsNot Nothing Then
                text = ReplaceString(text, sSearchString, sReplace, iReplacements)
            Else
                iReplacements = 1
            End If
            Return iReplacements
        End If

        Return Nothing
    End Function


    Friend Function FindStringInDescription(ByVal d As Description, ByVal sSearchString As String, Optional ByVal sReplace As String = Nothing, Optional ByVal bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As SingleDescription

        For Each sd As SingleDescription In d
            If FindText(sd.Description, sSearchString) Then
                If sReplace IsNot Nothing Then sd.Description = ReplaceString(sd.Description, sSearchString, sReplace, iReplacements)
                If Not bFindAll Then Return sd
            End If
            For Each r As clsRestriction In sd.Restrictions
                If r.oMessage IsNot Nothing Then
                    Dim sdr As SingleDescription = FindStringInDescription(r.oMessage, sSearchString, sReplace, bFindAll, iReplacements)
                    If sdr IsNot Nothing AndAlso Not bFindAll Then Return sdr
                End If
            Next
        Next
        Return Nothing

    End Function


    Private Function ReplaceString(ByRef sText As String, ByVal sFind As String, ByVal sReplace As String, ByRef iReplacements As Integer) As String
        Dim re As System.Text.RegularExpressions.Regex
        Dim sWordBound As String = CStr(IIf(SearchOptions.bFindExactWord, "\b", ""))
        sFind = sFind.Replace("\", "\\").Replace("*", ".*").Replace("?", "\?")
        If SearchOptions.bSearchMatchCase Then
            re = New System.Text.RegularExpressions.Regex(sWordBound & sFind & sWordBound)
        Else
            re = New System.Text.RegularExpressions.Regex(sWordBound & sFind & sWordBound, System.Text.RegularExpressions.RegexOptions.IgnoreCase)
        End If
        iReplacements += re.Matches(sText).Count
        sText = re.Replace(sText, sReplace)

        Return sText
    End Function

    Public Function SearchAndReplace(ByVal sFind As String, ByVal sReplace As String) As Integer
        Dim iReplacements As Integer = 0
        FindString(sFind, sReplace, True, iReplacements)
        Return iReplacements
    End Function

End Class


Public MustInherit Class clsItemWithProperties
    Inherits clsItem

    ' These are the properties that belong to the item
    Private _htblActualProperties As New PropertyHashTable
    Friend Property htblActualProperties As PropertyHashTable
        Get
            Return _htblActualProperties
        End Get
        Set(value As PropertyHashTable)
            _htblActualProperties = value
            _htblProperties = Nothing ' To Ensure any subsequent calls to htblProperties doesn't take stale values
        End Set
    End Property

    ' Any properties inherited from the group/class
    Private _htblInheritedProperties As PropertyHashTable
    Private Property htblInheritedProperties As PropertyHashTable
        Get
            If _htblInheritedProperties Is Nothing Then
                _htblInheritedProperties = New PropertyHashTable
                For Each grp As clsGroup In Adventure.htblGroups.Values
                    If grp.GroupType = PropertyGroupType Then
                        If grp.arlMembers.Contains(Key) Then
                            For Each prop As clsProperty In grp.htblProperties.Values
                                If Not _htblInheritedProperties.ContainsKey(prop.Key) Then _htblInheritedProperties.Add(prop.Copy)
                                With _htblInheritedProperties(prop.Key)
                                    .Value = prop.Value
                                    .StringData = prop.StringData.Copy
                                    .FromGroup = True
                                End With
                            Next
                        End If
                    End If
                Next
            End If
            Return _htblInheritedProperties
        End Get
        Set(value As PropertyHashTable)
            _htblInheritedProperties = value
            _htblProperties = Nothing
        End Set
    End Property

    Protected MustOverride ReadOnly Property PropertyGroupType() As clsGroup.GroupTypeEnum

    Friend MustOverride ReadOnly Property Parent As String

    ' Set this to False whenever the groups change
    'Friend bCalculatedGroups As Boolean = False


    ' This is a view of Actual + Inherited properties for the item
    ' If both lists have the same property, inherited one should take precendence if it has a value
    ' If it doesn't have a value, item property should take precendence
    '
    ' We can cache a view of the properties.  But if we add/delete any we need to recalc
    ' 
    Private _htblProperties As PropertyHashTable
    Friend ReadOnly Property htblProperties As PropertyHashTable
        Get
            If _htblProperties Is Nothing Then
                ' Need a shallow clone of actual properties (so we have unique list of original properties)
                _htblProperties = New PropertyHashTable
                For Each prop As clsProperty In _htblActualProperties.Values
                    _htblProperties.Add(prop)
                Next

                ' Take all actual properties, then layer on the inherited ones
                For Each prop As clsProperty In htblInheritedProperties.Values
                    If Not HasProperty(prop.Key) Then
                        _htblProperties.Add(prop.Copy)
                    Else
                        ' We have property in both actual and inherited.  Check what we want to do here...
                        With _htblProperties(prop.Key)
                            .Value = prop.Value
                            .StringData = prop.StringData.Copy
                            .FromGroup = True
                        End With
                    End If
                Next
            End If
            Return _htblProperties
        End Get
    End Property


    ' Required if we do an action that changes the properties of the object, such as add/remove from a group
    Friend Sub ResetInherited()
        _htblInheritedProperties = Nothing
        _htblProperties = Nothing
    End Sub

    Friend Function GetProperty(ByVal sPropertyKey As String) As clsProperty
        Return htblProperties(sPropertyKey)
    End Function


    Friend Function GetPropertyValue(ByVal sPropertyKey As String) As String
        If HasProperty(sPropertyKey) Then
            Return htblProperties(sPropertyKey).Value
        Else
            Return Nothing
        End If
    End Function


    Friend Sub SetPropertyValue(ByVal sPropertyKey As String, ByVal StringData As Description)
        AddProperty(sPropertyKey)
        htblProperties(sPropertyKey).StringData = StringData
    End Sub


    Friend Sub SetPropertyValue(ByVal sPropertyKey As String, ByVal sValue As String)
        AddProperty(sPropertyKey)
        htblProperties(sPropertyKey).Value = sValue
    End Sub


    Friend Sub SetPropertyValue(ByVal sPropertyKey As String, ByVal bValue As Boolean)
        ' Add/Remove the property from our collection
        If bValue Then
            AddProperty(sPropertyKey)
            htblProperties(sPropertyKey).Selected = True
        Else
            RemoveProperty(sPropertyKey)
        End If

    End Sub


    Friend Function HasProperty(ByVal sPropertyKey As String) As Boolean
        Return htblProperties.ContainsKey(sPropertyKey)
    End Function

    Friend Sub RemoveProperty(ByVal sPropertyKey As String)
        If _htblActualProperties.ContainsKey(sPropertyKey) Then
            _htblActualProperties.Remove(sPropertyKey)
            _htblProperties = Nothing
        End If
    End Sub

    Friend Sub AddProperty(ByVal prop As clsProperty, Optional ByVal bActualProperties As Boolean = True)
        If HasProperty(prop.Key) Then RemoveProperty(prop.Key)
        If bActualProperties Then
            _htblActualProperties.Add(prop)
        Else
            _htblInheritedProperties.Add(prop)
        End If
        _htblProperties = Nothing
    End Sub


    Friend Sub AddProperty(ByVal sPropertyKey As String, Optional ByVal bActualProperties As Boolean = True)
        If Not HasProperty(sPropertyKey) Then
            If Adventure.htblAllProperties.ContainsKey(sPropertyKey) Then
                AddProperty(Adventure.htblAllProperties(sPropertyKey).Copy, bActualProperties)
            Else
                Throw New Exception("Property " & sPropertyKey & " does not exist!")
            End If
        End If
    End Sub

    Friend Overrides ReadOnly Property AllDescriptions As System.Collections.Generic.List(Of SharedModule.Description)
        Get

        End Get
    End Property

    Public Overrides ReadOnly Property Clone As clsItem
        Get

        End Get
    End Property

    Public Overrides ReadOnly Property CommonName As String
        Get

        End Get
    End Property

    Public Overrides Function DeleteKey(sKey As String) As Boolean

    End Function

    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object

    End Function

    Public Overrides Function ReferencesKey(sKey As String) As Integer

    End Function
End Class