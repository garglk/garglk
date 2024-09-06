Public Class clsALR
    Inherits clsItem

    Private sOldText As String
    Private oNewText As Description

    Public Sub New()
        NewText = New Description
    End Sub
    Friend Property OldText() As String
        Get
            Return sOldText
        End Get
        Set(ByVal Value As String)
            sOldText = Value
        End Set
    End Property

    Friend Property NewText() As Description
        Get
            If oNewText Is Nothing Then oNewText = New Description
            Return oNewText
        End Get
        Set(ByVal Value As Description)
            oNewText = Value
        End Set
    End Property

    Public Overrides ReadOnly Property Clone() As clsItem
        Get
            Return CType(Me.MemberwiseClone, clsALR)
        End Get
    End Property

    Public Overrides ReadOnly Property CommonName() As String
        Get
            Return OldText
        End Get
    End Property

    Friend Overrides ReadOnly Property AllDescriptions() As Generic.List(Of SharedModule.Description)
        Get
            Dim all As New Generic.List(Of Description)
            all.Add(NewText)
            Return all
        End Get
    End Property

    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object
        Dim iCount As Integer = iReplacements
        iReplacements += MyBase.FindStringInStringProperty(Me.sOldText, sSearchString, sReplace, bFindAll)
        Return iReplacements - iCount
    End Function

    Public Overrides Function ReferencesKey(ByVal sKey As String) As Integer
        Dim iCount As Integer = 0
        For Each d As Description In AllDescriptions
            iCount += d.ReferencesKey(sKey)
        Next

        Return iCount
    End Function


    Public Overrides Function DeleteKey(ByVal sKey As String) As Boolean
        For Each d As Description In AllDescriptions
            If Not d.DeleteKey(sKey) Then Return False
        Next

        Return True
    End Function

End Class
