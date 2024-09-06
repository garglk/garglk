Public Class clsGroup
    Inherits clsItem

    Private sName As String

    Public Enum GroupTypeEnum
        Locations = 0
        Objects = 1
        Characters = 2
    End Enum
    Private eGroupType As GroupTypeEnum

    Friend htblProperties As New PropertyHashTable

    Private sarlMembers As New StringArrayList
    Friend Property arlMembers() As StringArrayList
        Get
            If Key = ALLROOMS Then
                sarlMembers.Clear()
                For Each sKey As String In Adventure.htblLocations.Keys
                    sarlMembers.Add(sKey)
                Next
            End If
            Return sarlMembers
        End Get
        Set(ByVal value As StringArrayList)
            sarlMembers = value
        End Set
    End Property

    Public Property Name() As String
        Get
            Return sName
        End Get
        Set(ByVal Value As String)
            sName = Value
        End Set
    End Property

    Public Property GroupType() As GroupTypeEnum
        Get
            Return eGroupType
        End Get
        Set(ByVal Value As GroupTypeEnum)
            eGroupType = Value
        End Set
    End Property

    Public ReadOnly Property RandomKey() As String
        Get
            Return arlMembers(Random(Me.arlMembers.Count - 1))
        End Get
    End Property
    Public Overrides ReadOnly Property Clone() As clsItem
        Get
            Return CType(Me.MemberwiseClone, clsGroup)
        End Get
    End Property

    Public Overrides ReadOnly Property CommonName() As String
        Get
            Return Name
        End Get
    End Property


    Friend Overrides ReadOnly Property AllDescriptions() As System.Collections.Generic.List(Of SharedModule.Description)
        Get
            Dim all As New Generic.List(Of Description)
            Return all
        End Get
    End Property


    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object
        Dim iCount As Integer = iReplacements
        iReplacements += MyBase.FindStringInStringProperty(Me.sName, sSearchString, sReplace, bFindAll)
        Return iReplacements - iCount
    End Function

    Public Overrides Function ReferencesKey(ByVal sKey As String) As Integer

        Dim iCount As Integer = 0
        For Each d As Description In AllDescriptions
            iCount += d.ReferencesKey(sKey)
        Next
        If Not Key = ALLROOMS AndAlso Me.arlMembers.Contains(sKey) Then iCount += 1
        iCount += htblProperties.ReferencesKey(sKey)

        Return iCount

    End Function

    Public Overrides Function DeleteKey(ByVal sKey As String) As Boolean

        For Each d As Description In AllDescriptions
            If Not d.DeleteKey(sKey) Then Return False
        Next
        If arlMembers.Contains(sKey) Then arlMembers.Remove(sKey)
        If Not htblProperties.DeleteKey(sKey) Then Return False

        Return True
    End Function

End Class
