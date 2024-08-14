Public Class clsHint
    Inherits clsItem

    Private sQuestion As String
    Private oSubtleHint As Description
    Private oSledgeHammerHint As Description
    Friend arlRestrictions As New RestrictionArrayList


    Public Sub New()
        oSubtleHint = New Description
        oSledgeHammerHint = New Description
    End Sub
    Public Overrides Function DeleteKey(ByVal sKey As String) As Boolean

        For Each d As Description In AllDescriptions
            If Not d.DeleteKey(sKey) Then Return False
        Next
        If Not arlRestrictions.DeleteKey(sKey) Then Return False

        Return (True)

    End Function


    Public Property Question() As String
        Get
            Return sQuestion
        End Get
        Set(ByVal Value As String)
            sQuestion = Value
        End Set
    End Property

    Friend Property SubtleHint() As Description
        Get
            Return oSubtleHint
        End Get
        Set(ByVal Value As Description)
            oSubtleHint = Value
        End Set
    End Property

    Friend Property SledgeHammerHint() As Description
        Get
            Return oSledgeHammerHint
        End Get
        Set(ByVal Value As Description)
            oSledgeHammerHint = Value
        End Set
    End Property

    Public Overrides ReadOnly Property Clone() As clsItem
        Get
            Return CType(Me.MemberwiseClone, clsHint)
        End Get
    End Property

    Public Overrides ReadOnly Property CommonName() As String
        Get
            Return Question
        End Get
    End Property

    Friend Overrides ReadOnly Property AllDescriptions() As System.Collections.Generic.List(Of SharedModule.Description)
        Get
            Dim all As New Generic.List(Of Description)
            all.Add(SledgeHammerHint)
            all.Add(SubtleHint)
            Return all
        End Get
    End Property


    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object
        Dim iCount As Integer = iReplacements
        iReplacements += MyBase.FindStringInStringProperty(Me.sQuestion, sSearchString, sReplace, bFindAll)
        Return iReplacements - iCount
    End Function

    Public Overrides Function ReferencesKey(ByVal sKey As String) As Integer

    End Function
End Class
