<DebuggerDisplay("Name: {Description}, Type: {Type}")>
Public Class clsProperty
    Inherits clsItem

    Public Enum PropertyOfEnum
        Locations = 0
        Objects = 1
        Characters = 2
        AnyItem = 3
    End Enum

    Public Enum PropertyTypeEnum
        SelectionOnly
        [Integer]
        Text
        ObjectKey
        StateList
        CharacterKey
        LocationKey
        LocationGroupKey
        ValueList
    End Enum


    Private eType As PropertyTypeEnum
    Private sDependentKey As String

    Friend arlStates As New StringArrayList
    Friend ValueList As New Dictionary(Of String, Integer)(System.StringComparer.OrdinalIgnoreCase)

    Friend Property PropertyOf As PropertyOfEnum
    Public Property PrivateTo As String = ""
    Public Property Mandatory() As Boolean
    Public Property FromGroup() As Boolean
    Public Property Description() As String
    Public Property AppendToProperty() As String
    Public Property GroupOnly As Boolean
    Public Property IntData() As Integer
    Friend Property StringData() As Description
    Public Property Selected() As Boolean
    Public Property Indent As Integer = 0
    Public Property DependentValue() As String
    Public Property RestrictProperty() As String
    Public Property RestrictValue() As String
    Public Property PopupDescription As String


    Friend Property Type() As PropertyTypeEnum
        Get
            Return eType
        End Get
        Set(ByVal Value As PropertyTypeEnum)
            eType = Value
            Select Case eType
                Case PropertyTypeEnum.StateList
                    If Not arlStates.Contains(StringData.ToString) AndAlso arlStates.Count > 0 Then
                        StringData = New Description(arlStates(0)) ' Default the value to the first state
                    End If
                Case PropertyTypeEnum.ValueList
                    If Not ValueList.ContainsValue(IntData) AndAlso ValueList.Count > 0 Then
                        For Each iValue As Integer In ValueList.Values
                            IntData = iValue
                            Exit For
                        Next
                    End If
            End Select
        End Set
    End Property

    Public Overrides ReadOnly Property Clone() As clsItem
        Get
            Return Copy()
        End Get
    End Property

    Public Property DependentKey() As String
        Get
            Return sDependentKey
        End Get
        Set(ByVal Value As String)
            If iLoading > 0 OrElse Value Is Nothing OrElse KeyExists(Value) Then
                sDependentKey = Value
            Else
                Throw New Exception("Key " & Value & " doesn't exist")
            End If
        End Set
    End Property

    Friend ReadOnly Property PossibleValues(Optional ByVal CurrentProperties As PropertyHashTable = Nothing) As StringArrayList
        Get
            Dim sValues As New StringArrayList

            Select Case Type
                Case clsProperty.PropertyTypeEnum.CharacterKey
                    For Each ch As clsCharacter In Adventure.htblCharacters.Values
                        If (RestrictProperty Is Nothing OrElse ch.HasProperty(RestrictProperty)) Then
                            If RestrictValue Is Nothing OrElse ch.GetPropertyValue(RestrictProperty) = RestrictValue Then
                                sValues.Add(ch.Key)
                            End If
                        End If
                    Next
                Case clsProperty.PropertyTypeEnum.Integer
                    TODO("Integer values")
                Case clsProperty.PropertyTypeEnum.LocationGroupKey
                    TODO("Location Group values")
                Case clsProperty.PropertyTypeEnum.LocationKey
                    For Each loc As clsLocation In Adventure.htblLocations.Values
                        If (RestrictProperty Is Nothing OrElse loc.HasProperty(RestrictProperty)) Then
                            If RestrictValue Is Nothing OrElse loc.GetPropertyValue(RestrictProperty) = RestrictValue Then
                                sValues.Add(loc.Key)
                            End If
                        End If
                    Next
                Case clsProperty.PropertyTypeEnum.ObjectKey
                    For Each ob As clsObject In Adventure.htblObjects.Values
                        If (RestrictProperty Is Nothing OrElse ob.HasProperty(RestrictProperty)) Then
                            If RestrictValue Is Nothing OrElse ob.GetPropertyValue(RestrictProperty) = RestrictValue Then
                                sValues.Add(ob.Key)
                            End If
                        End If
                    Next
                Case clsProperty.PropertyTypeEnum.SelectionOnly
                    sValues.Add(sSELECTED)
                    sValues.Add(sUNSELECTED) ' Not strictly a property value, but only way for action to remove the property
                Case clsProperty.PropertyTypeEnum.StateList
                    For Each sState As String In arlStates
                        sValues.Add(sState)
                    Next
                    If CurrentProperties Is Nothing Then CurrentProperties = Adventure.htblAllProperties
                    For Each prop As clsProperty In CurrentProperties.Values
                        If (CurrentProperties Is Adventure.htblAllProperties OrElse prop.Selected) AndAlso prop.Type = PropertyTypeEnum.StateList AndAlso prop.AppendToProperty = Me.Key Then
                            For Each sState As String In prop.arlStates
                                sValues.Add(sState)
                            Next
                        End If
                    Next
                Case clsProperty.PropertyTypeEnum.Text
                    TODO("Text values")
            End Select

            Return sValues
        End Get
    End Property



    Public Property Value(Optional ByVal bTesting As Boolean = False) As String
        Get
            Select Case eType
                Case PropertyTypeEnum.StateList, PropertyTypeEnum.ObjectKey, PropertyTypeEnum.CharacterKey, PropertyTypeEnum.LocationKey, PropertyTypeEnum.LocationGroupKey
                    Return StringData.ToString
                Case PropertyTypeEnum.ValueList
                    Return IntData.ToString
                Case PropertyTypeEnum.Text
                    Return StringData.ToString(bTesting)
                Case PropertyTypeEnum.Integer
                    Return IntData.ToString
                Case PropertyTypeEnum.SelectionOnly
                    Return True.ToString
            End Select
            Return Nothing
        End Get
        Set(ByVal Value As String)
            Try
                Select Case eType
                    Case PropertyTypeEnum.StateList
                        If iLoading > 0 OrElse Value Is Nothing OrElse PossibleValues.Contains(Value) Then
                            StringData = New Description(Value)
                        ElseIf PossibleValues.Count > 0 Then
                            ' Perhaps it's an expression that resolves to a valid state...
                            Dim v As New clsVariable
                            v.Type = clsVariable.VariableTypeEnum.Text
                            v.SetToExpression(Value)
                            If v.StringValue IsNot Nothing AndAlso PossibleValues.Contains(v.StringValue) Then
                                StringData = New Description(v.StringValue)
                            Else
                                Throw New Exception("'" & Value & "' is not a valid state.")
                            End If
                        End If
                    Case PropertyTypeEnum.ValueList
                        If iLoading > 0 OrElse Value Is Nothing OrElse ValueList.ContainsKey(Value) OrElse ValueList.ContainsValue(SafeInt(Value)) Then
                            If ValueList.ContainsKey(Value) Then
                                IntData = ValueList(Value)
                            Else
                                IntData = SafeInt(Value)
                            End If
                        ElseIf ValueList.Count > 0 Then
                            ' Perhaps it's an expression that resolves to a valid state...
                            Dim v As New clsVariable
                            v.Type = clsVariable.VariableTypeEnum.Text
                            v.SetToExpression(Value)
                            If v.StringValue IsNot Nothing AndAlso ValueList.ContainsKey(v.StringValue) Then
                                IntData = ValueList(v.StringValue)
                            Else
                                Throw New Exception("'" & Value & "' is not a valid state.")
                            End If
                        End If
                    Case PropertyTypeEnum.ObjectKey, PropertyTypeEnum.CharacterKey, PropertyTypeEnum.LocationKey, PropertyTypeEnum.LocationGroupKey
                        StringData = New Description(Value)
                    Case PropertyTypeEnum.Text
                        If StringData.Count < 2 Then
                            ' Simple text stored
                            If Not (StringData.Count = 1 AndAlso StringData(0).Description = Value) Then StringData = New Description(Value)
                        Else
                            ' We have a complex description here, probably with restrictions etc - if we set the property, we'll lose it                            
                        End If
                    Case PropertyTypeEnum.Integer
                        If IsNumeric(Value) Then
                            IntData = SafeInt(Value)
                        Else
                            Dim var As New clsVariable
                            var.Type = clsVariable.VariableTypeEnum.Numeric
                            var.SetToExpression(Value)
                            IntData = var.IntValue
                        End If
                End Select
            Catch ex As Exception
                ErrMsg("Error Setting Property " & Description & " to """ & Value & """", ex)
            End Try
        End Set
    End Property

    Public Function Copy() As clsProperty

        Dim p As New clsProperty

        With p
            .arlStates = Me.arlStates.Clone
            .ValueList.Clear()
            For Each sLabel As String In Me.ValueList.Keys
                .ValueList.Add(sLabel, Me.ValueList(sLabel))
            Next
            .DependentKey = Me.DependentKey
            .DependentValue = Me.DependentValue
            .AppendToProperty = Me.AppendToProperty
            .RestrictProperty = Me.RestrictProperty
            .RestrictValue = Me.RestrictValue
            .Description = Me.Description
            .IntData = Me.IntData
            .Key = Me.Key
            .Mandatory = Me.Mandatory
            .Selected = Me.Selected
            .Type = Me.Type ' Needs to be set before StringData, otherwise an appended StateList won't be allowed as a valid value (e.g. Locked will be reset to Open)
            .StringData = Me.StringData.Copy
            .PropertyOf = Me.PropertyOf
            .GroupOnly = Me.GroupOnly
            .FromGroup = Me.FromGroup
            .PrivateTo = Me.PrivateTo
            .PopupDescription = Me.PopupDescription

            Select Case .Type
                Case PropertyTypeEnum.ObjectKey, PropertyTypeEnum.CharacterKey, PropertyTypeEnum.LocationKey, PropertyTypeEnum.LocationGroupKey, PropertyTypeEnum.Text, PropertyTypeEnum.ValueList, PropertyTypeEnum.StateList
                    ' These will all re-write StringData, so we can ignore
                Case Else
                    .Value = Me.Value
            End Select
        End With

        Return p

    End Function

    Public Shadows Function ToString() As String
        Return Me.Description & " (" & Me.Value & ")"
    End Function

    Public Sub New()
        Me.StringData = New Description
        Me.PropertyOf = PropertyOfEnum.Objects
    End Sub

    Public Overrides ReadOnly Property CommonName() As String
        Get
            Return Description
        End Get
    End Property

    Friend Overrides ReadOnly Property AllDescriptions() As System.Collections.Generic.List(Of SharedModule.Description)
        Get
            Dim all As New Generic.List(Of Description)
            all.Add(Me.StringData)
            Return all
        End Get
    End Property

    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object
        Dim iCount As Integer = iReplacements
        iReplacements += MyBase.FindStringInStringProperty(Description, sSearchString, sReplace, bFindAll)
        Return iReplacements - iCount
    End Function

    Public Overrides Function ReferencesKey(ByVal sKey As String) As Integer
        Dim iCount As Integer = 0
        If Me.DependentKey = sKey Then iCount += 1
        If Me.RestrictProperty = sKey Then iCount += 1
        Return iCount
    End Function
    Public Overrides Function DeleteKey(ByVal sKey As String) As Boolean
        If Me.DependentKey = sKey Then Me.DependentKey = Nothing
        If Me.RestrictProperty = sKey Then Me.RestrictProperty = Nothing
        Return True
    End Function

End Class
