Public Class clsSynonym
    Inherits clsItem
    Friend Overrides ReadOnly Property AllDescriptions As System.Collections.Generic.List(Of SharedModule.Description)
        Get
            Dim all As New Generic.List(Of Description)
            Return all
        End Get
    End Property

    Public Overrides ReadOnly Property Clone As clsItem
        Get
            Return CType(Me.MemberwiseClone, clsSynonym)
        End Get
    End Property

    Public Overrides ReadOnly Property CommonName As String
        Get
            Dim sName As String = ""
            For Each sFrom As String In ChangeFrom
                If sName <> "" Then sName &= ", "
                sName &= sFrom
            Next
            sName &= " -> " & ChangeTo
            Return sName
        End Get
    End Property

    Public Overrides Function DeleteKey(sKey As String) As Boolean

        For Each d As Description In AllDescriptions
            If Not d.DeleteKey(sKey) Then Return False
        Next
        Return True

    End Function

    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object
        Dim iCount As Integer = iReplacements
        For Each sFrom As String In ChangeFrom
            iReplacements += MyBase.FindStringInStringProperty(sFrom, sSearchString, sReplace, bFindAll)
        Next
        iReplacements += MyBase.FindStringInStringProperty(sTo, sSearchString, sReplace, bFindAll)
        Return iReplacements - iCount
    End Function

    Public Overrides Function ReferencesKey(sKey As String) As Integer
        Dim iCount As Integer = 0
        For Each d As Description In AllDescriptions
            iCount += d.ReferencesKey(sKey)
        Next
        
        Return iCount
    End Function

    Private arlFrom As New StringArrayList
    Friend Property ChangeFrom As StringArrayList
        Get
            Return arlFrom
        End Get
        Set(value As StringArrayList)
            arlFrom = value
        End Set
    End Property

    Private sTo As String
    Public Property ChangeTo As String
        Get
            Return sTo
        End Get
        Set(value As String)
            sTo = value
        End Set
    End Property

End Class
