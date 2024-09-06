Imports FrankenDrift.Glue

Public Class clsVariable
    Inherits clsItem

    Private sName As String
    Private iIntValue(0) As Integer
    Private sStringValue(0) As String
    Private iLength As Integer = 1

    Public Enum VariableTypeEnum
        Numeric
        Text
    End Enum
    Private eVariableType As VariableTypeEnum

    Public Property Name() As String
        Get
            Return sName
        End Get
        Set(ByVal Value As String)
            sName = Value
        End Set
    End Property

    Public Overrides ReadOnly Property Clone() As clsItem
        Get
            Return CType(Me.MemberwiseClone, clsVariable)
        End Get
    End Property

    Public Property Length() As Integer
        Get
            Return iLength
        End Get
        Set(ByVal value As Integer)
            If Type = VariableTypeEnum.Numeric Then
                ReDim iIntValue(value - 1)
                ReDim sStringValue(0)
                sStringValue(0) = ""
            Else
                ReDim sStringValue(value - 1)
                Erase iIntValue
            End If
            iLength = value
        End Set
    End Property


    Public Property IntValue(Optional ByVal iIndex As Integer = 1) As Integer
        Get
            If iIndex > 0 AndAlso iIntValue IsNot Nothing AndAlso iIndex < iIntValue.Length + 1 Then
                Return iIntValue(iIndex - 1) ' 1 - based indices
            Else
                If iIntValue IsNot Nothing Then
                    ErrMsg("Attempting to read index " & iIndex & " outside bounds of array of variable " & Me.Name & IIf(iIntValue.Length > 1, "(" & iIntValue.Length & ")", "").ToString)
                Else
                    ErrMsg("Attempting to read index " & iIndex & " outside bounds of array of variable (0)")
                End If
                Return 0
            End If
        End Get
        Set(ByVal Value As Integer)
            If iIndex > 0 AndAlso iIndex < iIntValue.Length + 1 Then
                iIntValue(iIndex - 1) = Value
            Else
                ErrMsg("Attempting to set index " & iIndex & " outside bounds of array of variable " & Me.Name & IIf(iIntValue.Length > 1, "(" & iIntValue.Length & ")", "").ToString)
            End If
        End Set
    End Property


    Friend Property StringValue(Optional ByVal iIndex As Integer = 1) As String
        Get
            If iIndex > 0 AndAlso iIndex < sStringValue.Length + 1 Then
                Return sStringValue(iIndex - 1)
            Else
                ErrMsg("Attempting to read index " & iIndex & " outside bounds of array of variable " & Me.Name & IIf(sStringValue.Length > 1, "(" & sStringValue.Length & ")", "").ToString)
                Return ""
            End If
        End Get
        Set(ByVal Value As String)
            If iIndex > 0 AndAlso iIndex < sStringValue.Length + 1 Then
                sStringValue(iIndex - 1) = Value
            ElseIf iIndex <= 0 Then
                ErrMsg("Attempting to set index " & iIndex & " outside bounds of array of variable " & Me.Name & IIf(sStringValue.Length > 1, "(" & sStringValue.Length & ")", "").ToString & ".  ADRIFT arrays start at 1, not 0.")
            Else
                ErrMsg("Attempting to set index " & iIndex & " outside bounds of array of variable " & Me.Name & IIf(sStringValue.Length > 1, "(" & sStringValue.Length & ")", "").ToString)
            End If
        End Set
    End Property

    Public Property Type() As VariableTypeEnum
        Get
            Return eVariableType
        End Get
        Set(ByVal Value As VariableTypeEnum)
            eVariableType = Value
            Select Case Value
                Case VariableTypeEnum.Numeric
                    ReDim iIntValue(0)
                    ReDim sStringValue(0)
                    sStringValue(0) = ""
                Case VariableTypeEnum.Text
                    ReDim sStringValue(0)
                    Erase iIntValue
            End Select
        End Set
    End Property


#Region "Expressions"

    Structure TokenType
        Dim Token As String
        Dim Value As String
        Dim Left As Integer
        Dim Right As Integer
    End Structure
    Private Token() As TokenType
    Private iToken As Integer

    Private sParseString As String
    Private sParseToken As String


    Private Function GetToken(ByRef text As String) As String

        Dim n As Integer

        GetToken = Nothing

        Select Case sLeft(text, 1)
            Case "*", "/", "+", "-", "^", "(", ")", ","
                GetToken = sLeft(text, 1)
                sParseString = sRight(sParseString, sParseString.Length - 1)
                Exit Function
            Case "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" '0 To 9
                n = 1
                Dim bDP As Boolean = False
                While IsNumeric(sMid(text, n, 1)) OrElse (Not bDP AndAlso sMid(text, n, 1) = ".")
                    If sMid(text, n, 1) = "." Then bDP = True
                    n = n + 1
                End While
                n = n - 1
                GetToken = sLeft(text, n)
                sParseString = sRight(sParseString, sParseString.Length - n)
            Case Chr(34)
                If sInstr(2, text, Chr(34)) > 0 Then
                    GetToken = "vlu" & sMid(text, 2, sInstr(2, text, Chr(34)) - 2)
                    sParseString = sRight(sParseString, sParseString.Length - GetToken.Length + 1)
                End If
            Case "'"
                If sInstr(2, text, "'") > 0 Then
                    GetToken = "vlu" & sMid(text, 2, sInstr(2, text, "'") - 2)
                    sParseString = sRight(sParseString, sParseString.Length - GetToken.Length + 1)
                End If
            Case "%"
                For Each var As clsVariable In Adventure.htblVariables.Values
                    If var.Length > 1 AndAlso text.ToLower.StartsWith("%" & var.Name.ToLower & "[") Then
                        Dim sArgs As String = GetFunctionArgs(text.Substring(InStr(text.ToLower, "%" & var.Name.ToLower & "[") + var.Name.Length))
                        GetToken = "var-" & var.Key & "[" & ReplaceFunctions(sArgs) & "]"
                        sParseString = sParseString.Replace("%" & var.Name & "[" & sArgs & "]%", "")
                        Exit Function
                    End If
                    If sLeft(text, var.Name.Length + 2) = "%" & var.Name & "%" Then
                        GetToken = "var-" & var.Key
                        sParseString = sRight(sParseString, sParseString.Length - var.Name.Length - 2)
                        Exit Function
                    End If
                Next
                If sLeft(text.ToLower, 6) = "%loop%" Then
                    GetToken = "lop"
                    sParseString = sRight(sParseString, sParseString.Length - 6)
                End If
                If sLeft(text.ToLower, 10) = "%maxscore%" Then
                    GetToken = "mxs"
                    sParseString = sRight(sParseString, sParseString.Length - 10)
                End If
                If sLeft(text.ToLower, 8) = "%number%" Then
                    GetToken = "num"
                    sParseString = sRight(sParseString, sParseString.Length - 8)
                End If
                If sLeft(text.ToLower, 7) = "%score%" Then
                    GetToken = "sco"
                    sParseString = sRight(sParseString, sParseString.Length - 7)
                End If
                If sLeft(text.ToLower, 6) = "%time%" Then
                    GetToken = "tim"
                    sParseString = sRight(sParseString, sParseString.Length - 6)
                End If
                If sLeft(text.ToLower, 7) = "%turns%" Then
                    GetToken = "trn"
                    sParseString = sRight(sParseString, sParseString.Length - 7)
                End If
                If sLeft(text.ToLower, 9) = "%version%" Then
                    GetToken = "ver"
                    sParseString = sRight(sParseString, sParseString.Length - 9)
                End If
                If sLeft(text.ToLower, 6) = "%text%" Then
                    GetToken = "txt"
                    sParseString = sRight(sParseString, sParseString.Length - 6)
                End If
                If sLeft(text.ToLower, 8) = "%player%" Then
                    GetToken = "ply"
                    sParseString = sRight(sParseString, sParseString.Length - 8)
                End If
                For Each fn As String In FunctionNames()
                    If LCase(text).StartsWith("%" & fn.ToLower & "[") AndAlso text.ToLower.Contains("]%") Then
                        Dim sArgs As String = GetFunctionArgs(text.Substring(InStr(text.ToLower, "%" & fn & "[") + fn.Length + 1))
                        GetToken = "fun-%" & fn & "[" & sArgs & "]%"
                        sParseString = sParseString.Replace(GetToken.Substring(4), "")
                        Exit Function
                    End If
                Next
                For Each ref As String In ReferenceNames()
                    If sLeft(text.ToLower, ref.Length) = ref.ToLower Then
                        GetToken = "ref-" & ref
                        sParseString = sRight(sParseString, sParseString.Length - ref.Length)
                        Exit For
                    End If
                Next

            Case "|"
                If sLeft(text, 2) = "||" Then
                    GetToken = "OR"
                    sParseString = sRight(sParseString, sParseString.Length - 2)
                    Exit Function
                End If
                GetToken = "OR"
                sParseString = sRight(sParseString, sParseString.Length - 1)
            Case "&"
                If sLeft(text, 2) = "&&" Then
                    GetToken = "AND"
                    sParseString = sRight(sParseString, sParseString.Length - 2)
                    Exit Function
                End If
                GetToken = "AND"
                sParseString = sRight(sParseString, sParseString.Length - 1)
            Case "="
                If sLeft(text, 2) = "==" Then
                    GetToken = "EQ"
                    sParseString = sRight(sParseString, sParseString.Length - 2)
                    Exit Function
                End If
                GetToken = "EQ"
                sParseString = sRight(sParseString, sParseString.Length - 1)
            Case "<"
                If sLeft(text, 2) = "<>" Then
                    GetToken = "NE"
                    sParseString = sRight(sParseString, sParseString.Length - 2)
                    Exit Function
                End If
                If sLeft(text, 2) = "<=" Then
                    GetToken = "LE"
                    sParseString = sRight(sParseString, sParseString.Length - 2)
                    Exit Function
                End If
                GetToken = "LT"
                sParseString = sRight(sParseString, sParseString.Length - 1)
            Case ">"
                If sLeft(text, 2) = ">=" Then
                    GetToken = "GE"
                    sParseString = sRight(sParseString, sParseString.Length - 2)
                    Exit Function
                End If
                GetToken = "GT"
                sParseString = sRight(sParseString, sParseString.Length - 1)
            Case "!"
                If sLeft(text, 2) = "!=" Then
                    GetToken = "NE"
                    sParseString = sRight(sParseString, sParseString.Length - 2)
                    Exit Function
                End If
                GetToken = "!"
                sParseString = sRight(sParseString, sParseString.Length - 1)
            Case "a", "A"
                If LCase(sLeft(text, 3)) = "and" Then
                    GetToken = "AND"
                    sParseString = sRight(sParseString, sParseString.Length - 3)
                    Exit Function
                End If
                If LCase(sLeft(text, 3)) = "abs" Then
                    GetToken = "abs"
                    sParseString = sRight(sParseString, sParseString.Length - 3)
                    Exit Function
                End If
            Case "e", "E"
                If LCase(sLeft(text, 6)) = "either" Then
                    GetToken = "either"
                    sParseString = sRight(sParseString, sParseString.Length - 6)
                    Exit Function
                End If
            Case "i", "I"
                If LCase(sLeft(text, 2)) = "if" Then
                    GetToken = "if"
                    sParseString = sRight(sParseString, sParseString.Length - 2)
                    Exit Function
                End If
                If LCase(sLeft(text, 5)) = "instr" Then
                    GetToken = "ist"
                    sParseString = sRight(sParseString, sParseString.Length - 5)
                    Exit Function
                End If
            Case "l", "L"
                If LCase(sLeft(text, 5)) = "lower" Or LCase(sLeft(text, 5)) = "lcase" Then
                    GetToken = "lwr"
                    sParseString = sRight(sParseString, sParseString.Length - 5)
                    Exit Function
                End If
                If LCase(sLeft(text, 4)) = "left" Then
                    GetToken = "lft"
                    sParseString = sRight(sParseString, sParseString.Length - 4)
                    Exit Function
                End If
                If LCase(sLeft(text, 3)) = "len" Then
                    GetToken = "len"
                    sParseString = sRight(sParseString, sParseString.Length - 3)
                    Exit Function
                End If
            Case "m", "M"
                If LCase(sLeft(text, 3)) = "max" Then
                    GetToken = "max"
                    sParseString = sRight(sParseString, sParseString.Length - 3)
                    Exit Function
                End If
                If LCase(sLeft(text, 3)) = "min" Then
                    GetToken = "min"
                    sParseString = sRight(sParseString, sParseString.Length - 3)
                    Exit Function
                End If
                If LCase(sLeft(text, 3)) = "mod" Then
                    GetToken = "mod"
                    sParseString = sRight(sParseString, sParseString.Length - 3)
                    Exit Function
                End If
                If LCase(sLeft(text, 3)) = "mid" Then
                    GetToken = "mid"
                    sParseString = sRight(sParseString, sParseString.Length - 3)
                    Exit Function
                End If
            Case "o", "O"
                If LCase(sLeft(text, 2)) = "or" Then
                    GetToken = "OR"
                    sParseString = sRight(sParseString, sParseString.Length - 2)
                    Exit Function
                End If
                If LCase(sLeft(text, 5)) = "oneof" Then
                    GetToken = "oneof"
                    sParseString = sRight(sParseString, sParseString.Length - 5)
                    Exit Function
                End If
            Case "p", "P"
                If LCase(sLeft(text, 6)) = "proper" Or LCase(sLeft(text, 5)) = "pcase" Then
                    GetToken = "ppr"
                    sParseString = sRight(sParseString, sParseString.Length - 5)
                    Exit Function
                End If
            Case "r", "R"
                If LCase(sLeft(text, 4)) = "rand" Then
                    GetToken = "rand"
                    sParseString = sRight(sParseString, sParseString.Length - 4)
                    Exit Function
                End If
                If LCase(sLeft(text, 7)) = "replace" Then
                    GetToken = "replace"
                    sParseString = sRight(sParseString, sParseString.Length - 7)
                    Exit Function
                End If
                If LCase(sLeft(text, 5)) = "right" Then
                    GetToken = "rgt"
                    sParseString = sRight(sParseString, sParseString.Length - 5)
                    Exit Function
                End If
            Case "s", "S"
                If LCase(sLeft(text, 3)) = "str" Then
                    GetToken = "str"
                    sParseString = sRight(sParseString, sParseString.Length - 3)
                    Exit Function
                End If
            Case "u", "U"
                If LCase(sLeft(text, 5)) = "upper" Or LCase(sLeft(text, 5)) = "ucase" Then
                    GetToken = "upr"
                    sParseString = sRight(sParseString, sParseString.Length - 5)
                    Exit Function
                End If
                If LCase(sLeft(text, 5)) = "urand" Then
                    GetToken = "urand"
                    sParseString = sRight(sParseString, sParseString.Length - 5)
                    Exit Function
                End If
            Case "v", "V"
                If LCase(sLeft(text, 3)) = "val" Then
                    GetToken = "val"
                    sParseString = sRight(sParseString, sParseString.Length - 3)
                    Exit Function
                End If
            Case Else

        End Select

        If GetToken Is Nothing Then GetToken = ""
        If GetToken = "" Then
            ' Treat the token like a string
            For i As Integer = 0 To sParseString.Length - 1
                Select Case sParseString(i)
                    Case "A"c To "Z"c, "a"c To "z"c, "0"c To "9"c
                        GetToken &= sParseString(i)
                    Case Else
                        sParseString = sRight(sParseString, sParseString.Length - GetToken.Length)
                        GetToken = "vlu" & GetToken
                        Exit Function
                End Select
            Next
        End If
    End Function


    Private Sub PrintTokens(Optional ByVal CurrentToken As TokenType = Nothing)
        Dim iToken As Integer = GetFirstToken() '0
        While iToken <> -1
            iToken = PrintToken(iToken, CurrentToken)
        End While
        Debug.WriteLine("")
    End Sub

    Private Function PrintToken(ByVal iToken As Integer, Optional ByVal CurrentToken As TokenType = Nothing) As Integer
        With Token(iToken)
            Dim bCurrent As Boolean = .Left = CurrentToken.Left AndAlso .Right = CurrentToken.Right AndAlso .Token = CurrentToken.Token
            Debug.Write("[" & If(bCurrent, "#", "").ToString & iToken & If(bCurrent, "#", "").ToString & ": " & .Token & "/" & .Value & "] ")
            Return .Right
        End With
    End Function

    Private Function GetFirstToken() As Integer
        For i As Integer = Token.Length - 1 To 0 Step -1
            If Token(i).Left = -1 Then Return i
        Next
    End Function

    Private Function NoRepeatRandom(ByVal iMin As Integer, ByVal iMax As Integer) As Integer
        Dim sDictKey As String = iMin & "-" & iMax

        If Not Adventure.dictRandValues.ContainsKey(sDictKey) Then Adventure.dictRandValues.Add(sDictKey, New List(Of Integer))

        Dim lValues As List(Of Integer) = Adventure.dictRandValues(sDictKey)
        If lValues.Count = 0 Then
            ' Create a new random list of values
            For i As Integer = iMin To iMax
                lValues.Insert(Random(lValues.Count), i)
            Next
        End If

        ' Pick a random element from the list
        Dim iIndex As Integer = Random(lValues.Count - 1)
        NoRepeatRandom = lValues(iIndex)
        lValues.RemoveAt(iIndex)
    End Function

    Public Sub SetToExpression(ByVal sExpression As String, Optional ByVal iIndex As Integer = 1, Optional ByVal bThrowExceptionOnBadExpression As Boolean = False)
        Try
            sExpression = sExpression.Replace("%Loop%", iIndex.ToString)
            sExpression = ReplaceFunctions(sExpression, True)
            ' Otherwise IF(%object%="Object1", "a", "b") becomes IF(Object1="Object1"...
            ' But it is needed so we can evaluate expressions like %object%.Weight+%object%.Children.Weight

            ' Remove spaces
            sExpression = sExpression.Replace("\""", "~@&#~")

            sParseString = StripRedundantSpaces(sExpression) ' Replace(expression, " ", "")
            Dim sPrevious As String = "972398"

            ' Set up initial tokens
            iToken = 0
            While sParseString <> "" AndAlso sParseString <> sPrevious
                sPrevious = sParseString
                ReDim Preserve Token(iToken + 1)
                If iToken > 0 Then Token(iToken - 1).Right = iToken
                With Token(iToken)
                    .Token = GetToken(sParseString)
                    If .Token IsNot Nothing Then .Token = .Token.Replace("~@&#~", """")
                    .Value = "0"
                    .Left = -1
                    .Right = -1
                    If iToken > 0 Then .Left = iToken - 1
                End With
                iToken = iToken + 1
            End While

            Dim n As Integer

            If iToken = 0 Then Exit Sub

            ' Set initial tokens to normal ones
            For n = 0 To iToken - 1
                With Token(n)
                    If IsNumeric(.Token) Then
                        .Value = .Token
                        .Token = "expr"
                    Else
                        Select Case .Token
                            Case "*", "/", "+", "-", "^", "mod"
                                .Value = .Token
                                .Token = "op"
                            Case "("
                                .Value = .Token
                                .Token = "lp"
                            Case ")"
                                .Value = .Token
                                .Token = "rp"
                            Case ","
                                .Value = .Token
                                .Token = "comma"
                            Case "txt"
                                .Value = ""
                                For Each ref As clsNewReference In UserSession.NewReferences
                                    If ref.ReferenceType = ReferencesType.Text Then
                                        If ref.Items.Count = 1 Then
                                            If ref.Items(0).MatchingPossibilities.Count = 1 Then
                                                .Value = ref.Items(0).MatchingPossibilities(0)
                                            End If
                                        End If
                                    End If
                                Next
                                .Token = "expr"
                            Case "min", "max", "if", "rand", "either", "abs", "upr", "lwr", "ppr", "rgt", "lft", "mid", "ist", "len", "val", "str", "replace", "oneof", "urand"
                                .Value = .Token
                                .Token = "funct"
                            Case "EQ", "GT", "LT", "LE", "GE", "NE"
                                .Value = .Token
                                .Token = "TESTOP"
                            Case "AND", "OR"
                                .Value = .Token
                                .Token = "LOGIC"
                            Case "lop"
                                .Value = iIndex.ToString
                                .Token = "expr"
                            Case "num"
                                .Value = ""
                                For Each ref As clsNewReference In UserSession.NewReferences
                                    If ref.ReferenceType = ReferencesType.Number Then
                                        If ref.Items.Count = 1 Then
                                            If ref.Items(0).MatchingPossibilities.Count = 1 Then
                                                .Value = ref.Items(0).MatchingPossibilities(0)
                                            End If
                                        End If
                                    End If
                                Next
                                .Token = "expr"
                            Case "ver"
                                .Value = Application.ProductVersion
                                Dim sVer As String() = Application.ProductVersion.Split("."c)
                                .Value = sVer(0) & CInt(sVer(1)).ToString("00") & CInt(sVer(2)).ToString("0000")
                                .Token = "expr"
                            Case "sco"
                                .Value = Adventure.Score.ToString
                                .Token = "expr"
                            Case "mxs"
                                .Value = Adventure.MaxScore.ToString
                                .Token = "expr"
                            Case "ply"
                                .Value = Adventure.Player.Name
                                .Token = "expr"
                            Case "trn"
                                .Value = Adventure.Turns.ToString
                                .Token = "expr"
                            Case "tim"
                                .Value = "0"
                                .Token = "expr"
                            Case Else
                                If sLeft(.Token, 3) = "var" Then
                                    Dim sVar As String = .Token.Replace("var-", "")
                                    Dim iVarIndex As Integer = 1
                                    If sVar.Contains("[") Then
                                        Dim sIndex As String = sVar.Substring(InStr(sVar, "["))
                                        sIndex = sLeft(sIndex, sIndex.Length - 1)
                                        iVarIndex = SafeInt(sIndex)
                                        sVar = sVar.Replace("[" & sIndex & "]", "")
                                    End If
                                    Dim var As clsVariable = Adventure.htblVariables(sVar)
                                    If var.Type = VariableTypeEnum.Text Then
                                        .Value = var.StringValue(iVarIndex).ToString
                                    Else
                                        .Value = var.IntValue(iVarIndex).ToString
                                    End If
                                    .Token = "expr"
                                ElseIf sLeft(.Token, 3) = "vlu" Then
                                    .Value = sRight(.Token, Len(.Token) - 3)
                                    .Token = "expr"
                                ElseIf sLeft(.Token, 3) = "prv" Then
                                    .Value = ReplaceFunctions(.Token.Substring(4))
                                    .Token = "expr"
                                ElseIf sLeft(.Token, 3) = "pin" Then
                                    .Value = ReplaceFunctions(.Token.Substring(4))
                                    .Token = "expr"
                                ElseIf sLeft(.Token, 3) = "pch" Then
                                    .Value = ReplaceFunctions(.Token.Substring(4))
                                    .Token = "expr"
                                ElseIf sLeft(.Token, 3) = "ref" Then
                                    .Value = ReplaceFunctions(.Token.Substring(4))
                                    .Token = "expr"
                                ElseIf sLeft(.Token, 4) = "fun-" Then
                                    .Value = ReplaceFunctions(.Token.Substring(4))
                                    .Token = "expr"
                                Else
                                    Throw New Exception("Bad token: " & .Token)
                                End If
                        End Select
                    End If
                End With
            Next n

            Dim lefttoken As TokenType, optoken As TokenType, righttoken As TokenType
            Dim currtoken As TokenType, lptoken As TokenType, rptoken As TokenType, functoken As TokenType
            Dim op2token As TokenType, right2token As TokenType
            Dim changed As Boolean
            Dim badexp As Boolean
            Dim run As Integer

            currtoken = Token(GetFirstToken)
            run = 1

            ' Do while more than one token
            While Not (currtoken.Left = -1 And currtoken.Right = -1)
                changed = False

                ' If at least two tokens
                If currtoken.Right <> -1 Then
                    ' If at least three tokens
                    If Token(currtoken.Right).Right <> -1 Then

                        If Token(Token(currtoken.Right).Right).Right <> -1 Then
                            ' Four tokens - for abs(a)
                            functoken = currtoken
                            lptoken = Token(currtoken.Right)
                            lefttoken = Token(lptoken.Right)
                            rptoken = Token(lefttoken.Right)

                            If functoken.Token = "funct" And lptoken.Token = "lp" And
                                lefttoken.Token = "expr" And rptoken.Token = "rp" Then
                                Select Case functoken.Value
                                    Case "abs"
                                        addtoken("expr", Math.Abs(CInt(Val(lefttoken.Value))).ToString, functoken.Left, rptoken.Right)
                                    Case "upr"
                                        addtoken("expr", UCase(lefttoken.Value), functoken.Left, rptoken.Right)
                                    Case "lwr"
                                        addtoken("expr", LCase(lefttoken.Value), functoken.Left, rptoken.Right)
                                    Case "ppr"
                                        addtoken("expr", ToProper(lefttoken.Value), functoken.Left, rptoken.Right)
                                    Case "len"
                                        addtoken("expr", lefttoken.Value.Length.ToString, functoken.Left, rptoken.Right)
                                    Case "val"
                                        addtoken("expr", SafeInt(Val(lefttoken.Value)).ToString, functoken.Left, rptoken.Right)
                                    Case "str"
                                        addtoken("expr", Str(lefttoken.Value), functoken.Left, rptoken.Right)
                                    Case "rand"
                                        addtoken("expr", Random(0, SafeInt(lefttoken.Value)).ToString, functoken.Left, rptoken.Right)
                                    Case "urand"
                                        addtoken("expr", NoRepeatRandom(0, SafeInt(lefttoken.Value)).ToString, functoken.Left, rptoken.Right)
                                End Select
                                If functoken.Left <> -1 Then Token(functoken.Left).Right = iToken - 1
                                If rptoken.Right <> -1 Then Token(rptoken.Right).Left = iToken - 1
                                changed = True
                            End If

                            If Token(Token(Token(currtoken.Right).Right).Right).Right <> -1 Then
                                If Token(Token(Token(Token(currtoken.Right).Right).Right).Right).Right <> -1 Then

                                    If Token(Token(Token(Token(Token(currtoken.Right).Right).Right).Right).Right).Right <> -1 Then
                                        If Token(Token(Token(Token(Token(Token(currtoken.Right).Right).Right).Right).Right).Right).Right <> -1 Then
                                            ' Eight tokens - for if(test, a, b)
                                            functoken = currtoken
                                            lptoken = Token(currtoken.Right)
                                            lefttoken = Token(lptoken.Right)
                                            optoken = Token(lefttoken.Right)
                                            righttoken = Token(optoken.Right)
                                            op2token = Token(righttoken.Right)
                                            right2token = Token(op2token.Right)
                                            rptoken = Token(right2token.Right)

                                            If functoken.Token = "funct" AndAlso lptoken.Token = "lp" AndAlso (lefttoken.Token = "test" OrElse lefttoken.Token = "expr") _
                                                AndAlso optoken.Token = "comma" AndAlso righttoken.Token = "expr" AndAlso op2token.Token = "comma" _
                                                AndAlso right2token.Token = "expr" AndAlso rptoken.Token = "rp" Then

                                                Select Case functoken.Value
                                                    Case "if"
                                                        If Val(lefttoken.Value) > 0 Then ' True
                                                            addtoken("expr", righttoken.Value, functoken.Left, rptoken.Right)
                                                        Else ' False
                                                            addtoken("expr", right2token.Value, functoken.Left, rptoken.Right)
                                                        End If
                                                End Select
                                                If functoken.Left <> -1 Then Token(functoken.Left).Right = iToken - 1
                                                If rptoken.Right <> -1 Then Token(rptoken.Right).Left = iToken - 1
                                                changed = True
                                            End If

                                            If functoken.Token = "funct" And lptoken.Token = "lp" And lefttoken.Token = "expr" _
                                                And optoken.Token = "comma" And righttoken.Token = "expr" And op2token.Token = "comma" _
                                                And right2token.Token = "expr" And rptoken.Token = "rp" Then

                                                Select Case functoken.Value
                                                    Case "mid"
                                                        addtoken("expr", sMid(lefttoken.Value, CInt(Val(righttoken.Value)), CInt(Val(right2token.Value))), functoken.Left, rptoken.Right)
                                                    Case "replace"
                                                        addtoken("expr", Replace(lefttoken.Value, righttoken.Value, right2token.Value), functoken.Left, rptoken.Right)
                                                End Select
                                                If functoken.Left <> -1 Then Token(functoken.Left).Right = iToken - 1
                                                If rptoken.Right <> -1 Then Token(rptoken.Right).Left = iToken - 1
                                                changed = True
                                            End If

                                        End If
                                    End If

                                    ' Six tokens - for funct(a,b)
                                    functoken = currtoken
                                    lptoken = Token(currtoken.Right)
                                    lefttoken = Token(lptoken.Right)
                                    optoken = Token(lefttoken.Right)
                                    righttoken = Token(optoken.Right)
                                    rptoken = Token(righttoken.Right)

                                    If functoken.Token = "funct" And lptoken.Token = "lp" And lefttoken.Token = "expr" _
                                        And optoken.Token = "comma" And righttoken.Token = "expr" And rptoken.Token = "rp" Then
                                        Select Case functoken.Value
                                            Case "max"
                                                addtoken("expr", Math.Max(CInt(Val(lefttoken.Value)), CInt(Val(righttoken.Value))).ToString, functoken.Left, rptoken.Right)
                                            Case "min"
                                                addtoken("expr", Math.Min(CInt(Val(lefttoken.Value)), CInt(Val(righttoken.Value))).ToString, functoken.Left, rptoken.Right)
                                            Case "either"
                                                If Random(1) = 1 Then
                                                    addtoken("expr", lefttoken.Value, functoken.Left, rptoken.Right)
                                                Else
                                                    addtoken("expr", righttoken.Value, functoken.Left, rptoken.Right)
                                                End If
                                            Case "rand"
                                                addtoken("expr", Random(SafeInt(lefttoken.Value), SafeInt(righttoken.Value)).ToString, functoken.Left, rptoken.Right)
                                            Case "urand"
                                                addtoken("expr", NoRepeatRandom(SafeInt(lefttoken.Value), SafeInt(righttoken.Value)).ToString, functoken.Left, rptoken.Right)
                                            Case "lft"
                                                addtoken("expr", sLeft(lefttoken.Value, CInt(Val(righttoken.Value))), functoken.Left, rptoken.Right)
                                            Case "rgt"
                                                addtoken("expr", sRight(lefttoken.Value, CInt(Val(righttoken.Value))), functoken.Left, rptoken.Right)
                                            Case "ist"
                                                addtoken("expr", sInstr(lefttoken.Value, righttoken.Value).ToString, functoken.Left, rptoken.Right)
                                        End Select
                                        If functoken.Left <> -1 Then Token(functoken.Left).Right = iToken - 1
                                        If rptoken.Right <> -1 Then Token(rptoken.Right).Left = iToken - 1
                                        changed = True
                                    End If



                                    If functoken.Token = "funct" And lptoken.Token = "lp" AndAlso lefttoken.Token = "expr" AndAlso functoken.Value = "oneof" Then
                                        ' Can have any number of arguments: oneof([expr,])

                                        Dim iLength As Integer = 1
                                        Dim oToken As TokenType = lefttoken

                                        While oToken.Right <> -1
                                            Dim oNext As TokenType = Token(oToken.Right)

                                            Select Case oNext.Token
                                                Case "rp"
                                                    ' Great, we reached the end of the function.  Let's evaluate
                                                    rptoken = oNext
                                                    Dim iOneOfIndex As Integer = Random(iLength - 1) + 1
                                                    Dim oRandom As TokenType = functoken
                                                    For i As Integer = 1 To iOneOfIndex
                                                        oRandom = Token(Token(oRandom.Right).Right)
                                                    Next
                                                    addtoken("expr", oRandom.Value, functoken.Left, rptoken.Right)
                                                    If functoken.Left <> -1 Then Token(functoken.Left).Right = iToken - 1
                                                    If rptoken.Right <> -1 Then Token(rptoken.Right).Left = iToken - 1
                                                    changed = True
                                                    Exit While
                                                Case "comma"
                                                    Dim oExpr As TokenType = Token(oNext.Right)
                                                    If oExpr.Token = "expr" Then
                                                        iLength += 1
                                                        oToken = Token(oNext.Right)
                                                    Else
                                                        ' Not what we were expecting
                                                        Exit While
                                                    End If
                                                Case Else
                                                    ' Ok, not what we were expecting
                                                    Exit While
                                            End Select
                                        End While

                                    End If


                                End If
                            End If
                        End If

                        lefttoken = currtoken
                        optoken = Token(currtoken.Right)
                        righttoken = Token(optoken.Right)

                        ' expr op expr
                        If lefttoken.Token = "expr" And optoken.Token = "op" And righttoken.Token = "expr" Then
                            Select Case optoken.Value
                                Case "^"
                                    addtoken("expr", (Val(lefttoken.Value) ^ Val(righttoken.Value)).ToString, lefttoken.Left, righttoken.Right)
                                Case "*"
                                    addtoken("expr", (Val(lefttoken.Value) * Val(righttoken.Value)).ToString, lefttoken.Left, righttoken.Right)
                                Case "/"
                                    addtoken("expr", (Math.Round((Val(lefttoken.Value) / Val(righttoken.Value)), MidpointRounding.AwayFromZero)).ToString, lefttoken.Left, righttoken.Right)
                                Case "+"
                                    If IsNumeric(lefttoken.Value) And IsNumeric(righttoken.Value) Then
                                        If run = 2 Then addtoken("expr", (Val(lefttoken.Value) + Val(righttoken.Value)).ToString, lefttoken.Left, righttoken.Right)
                                    Else
                                        addtoken("expr", lefttoken.Value & righttoken.Value, lefttoken.Left, righttoken.Right)
                                        run = 2
                                    End If
                                Case "-"
                                    If run = 2 Then addtoken("expr", (Val(lefttoken.Value) - Val(righttoken.Value)).ToString, lefttoken.Left, righttoken.Right)
                                Case "mod"
                                    If run = 2 Then addtoken("expr", (Val(lefttoken.Value) Mod Val(righttoken.Value)).ToString, lefttoken.Left, righttoken.Right)

                            End Select
                            If run = 2 Or optoken.Value = "*" Or optoken.Value = "/" Then
                                If lefttoken.Left <> -1 Then Token(lefttoken.Left).Right = iToken - 1
                                If righttoken.Right <> -1 Then Token(righttoken.Right).Left = iToken - 1
                                changed = True
                            End If
                        End If

                        If lefttoken.Token = "test" And optoken.Token = "LOGIC" And righttoken.Token = "test" Then
                            Select Case optoken.Value
                                Case "AND"
                                    If Val(lefttoken.Value) > 0 And Val(righttoken.Value) > 0 Then
                                        addtoken("test", "1", lefttoken.Left, righttoken.Right)
                                    Else
                                        addtoken("test", "0", lefttoken.Left, righttoken.Right)
                                    End If
                                Case "OR"
                                    If Val(lefttoken.Value) > 0 Or Val(righttoken.Value) > 0 Then
                                        addtoken("test", "1", lefttoken.Left, righttoken.Right)
                                    Else
                                        addtoken("test", "0", lefttoken.Left, righttoken.Right)
                                    End If
                            End Select
                            If lefttoken.Left <> -1 Then Token(lefttoken.Left).Right = iToken - 1
                            If righttoken.Right <> -1 Then Token(righttoken.Right).Left = iToken - 1
                            changed = True
                        End If

                        ' expr TESTOP expr
                        ' Do this on run 3 in case we have expr TESTOP expr OP expr, to ensure the expr OP expr reduces to expr first
                        If lefttoken.Token = "expr" And optoken.Token = "TESTOP" And righttoken.Token = "expr" And run = 3 Then
                            Select Case optoken.Value
                                Case "EQ"
                                    If IsNumeric(lefttoken.Value) AndAlso IsNumeric(righttoken.Value) Then
                                        If Val(lefttoken.Value) = Val(righttoken.Value) Then
                                            addtoken("test", "1", lefttoken.Left, righttoken.Right)
                                        Else
                                            addtoken("test", "0", lefttoken.Left, righttoken.Right)
                                        End If
                                    Else
                                        If lefttoken.Value = righttoken.Value Then
                                            addtoken("test", "1", lefttoken.Left, righttoken.Right)
                                        Else
                                            addtoken("test", "0", lefttoken.Left, righttoken.Right)
                                        End If
                                    End If
                                Case "NE"
                                    If IsNumeric(lefttoken.Value) AndAlso IsNumeric(righttoken.Value) Then
                                        If Val(lefttoken.Value) <> Val(righttoken.Value) Then
                                            addtoken("test", "1", lefttoken.Left, righttoken.Right)
                                        Else
                                            addtoken("test", "0", lefttoken.Left, righttoken.Right)
                                        End If
                                    Else
                                        If lefttoken.Value <> righttoken.Value Then
                                            addtoken("test", "1", lefttoken.Left, righttoken.Right)
                                        Else
                                            addtoken("test", "0", lefttoken.Left, righttoken.Right)
                                        End If
                                    End If
                                Case "GT"
                                    If Val(lefttoken.Value) > Val(righttoken.Value) Then
                                        addtoken("test", "1", lefttoken.Left, righttoken.Right)
                                    Else
                                        addtoken("test", "0", lefttoken.Left, righttoken.Right)
                                    End If
                                Case "LT"
                                    If Val(lefttoken.Value) < Val(righttoken.Value) Then
                                        addtoken("test", "1", lefttoken.Left, righttoken.Right)
                                    Else
                                        addtoken("test", "0", lefttoken.Left, righttoken.Right)
                                    End If
                                Case "GE"
                                    If Val(lefttoken.Value) >= Val(righttoken.Value) Then
                                        addtoken("test", "1", lefttoken.Left, righttoken.Right)
                                    Else
                                        addtoken("test", "0", lefttoken.Left, righttoken.Right)
                                    End If
                                Case "LE"
                                    If Val(lefttoken.Value) <= Val(righttoken.Value) Then
                                        addtoken("test", "1", lefttoken.Left, righttoken.Right)
                                    Else
                                        addtoken("test", "0", lefttoken.Left, righttoken.Right)
                                    End If
                            End Select
                            If lefttoken.Left <> -1 Then Token(lefttoken.Left).Right = iToken - 1
                            If righttoken.Right <> -1 Then Token(righttoken.Right).Left = iToken - 1
                            changed = True
                        End If

                        ' Concatenate text, e.g. "one" & "two"
                        ' Need to be careful that it evaluates late so we don't get: expr = expr AND expr = expr  reduced to expr = expr = expr
                        ' Logic should be the last thing to resolve
                        ' Should we somehow check that each expression is a string?
                        If lefttoken.Token = "expr" And optoken.Value = "AND" And righttoken.Token = "expr" And run = 3 Then
                            addtoken("expr", lefttoken.Value & righttoken.Value, lefttoken.Left, righttoken.Right)
                            If lefttoken.Left <> -1 Then Token(lefttoken.Left).Right = iToken - 1
                            If righttoken.Right <> -1 Then Token(righttoken.Right).Left = iToken - 1
                            changed = True
                        End If

                        ' ( expr )
                        If lefttoken.Token = "lp" And optoken.Token = "expr" And righttoken.Token = "rp" Then
                            If IsNumeric(optoken.Value) Then
                                addtoken("expr", Val(optoken.Value).ToString, lefttoken.Left, righttoken.Right)
                            Else
                                addtoken("expr", optoken.Value, lefttoken.Left, righttoken.Right)
                            End If

                            If lefttoken.Left <> -1 Then Token(lefttoken.Left).Right = iToken - 1
                            If righttoken.Right <> -1 Then Token(righttoken.Right).Left = iToken - 1
                            changed = True
                        End If

                        ' ( test )
                        If lefttoken.Token = "lp" And optoken.Token = "test" And righttoken.Token = "rp" Then
                            If IsNumeric(optoken.Value) Then
                                addtoken("test", Val(optoken.Value).ToString, lefttoken.Left, righttoken.Right)
                            Else
                                addtoken("test", optoken.Value, lefttoken.Left, righttoken.Right)
                            End If

                            If lefttoken.Left <> -1 Then Token(lefttoken.Left).Right = iToken - 1
                            If righttoken.Right <> -1 Then Token(righttoken.Right).Left = iToken - 1
                            changed = True
                        End If

                    End If

                    ' Only two tokens
                    optoken = currtoken
                    righttoken = Token(optoken.Right)

                    ' op expr
                    If optoken.Token = "op" And righttoken.Token = "expr" Then
                        Select Case optoken.Value
                            Case "-"
                                If run = 2 Then addtoken("expr", (-Val(righttoken.Value)).ToString, optoken.Left, righttoken.Right)
                            Case "+" ' Not really expected, but I guess +X = X
                                If run = 2 Then addtoken("expr", (Val(righttoken.Value)).ToString, optoken.Left, righttoken.Right)
                        End Select
                        If run = 2 Then
                            If optoken.Left <> -1 Then Token(optoken.Left).Right = iToken - 1
                            If righttoken.Right <> -1 Then Token(righttoken.Right).Left = iToken - 1
                            changed = True
                        End If

                    End If

                End If

                If changed Then
                    ' Move pointer to far left
                    currtoken = Token(iToken - 1)
                    Dim iBombOut As Integer = 0
                    While currtoken.Left <> -1
                        currtoken = Token(currtoken.Left)
                        iBombOut += 1
                        If iBombOut = 5000 Then
                            badexp = False
                            GoTo nxt
                        End If
                    End While
                    badexp = False
                    run = 1
                Else
                    If currtoken.Right = -1 Then
                        If badexp Then
                            If run = 1 Then
                                run = 2
                                currtoken = Token(iToken - 1)
                                Dim iBombOut As Integer = 0
                                While currtoken.Left <> -1
                                    currtoken = Token(currtoken.Left)
                                    iBombOut += 1
                                    If iBombOut = 5000 Then
                                        badexp = False
                                        GoTo nxt
                                    End If
                                End While
                                badexp = False
                                GoTo nxt
                            ElseIf run = 2 Then
                                run = 3
                                currtoken = Token(iToken - 1)
                                Dim iBombOut As Integer = 0
                                While currtoken.Left <> -1
                                    currtoken = Token(currtoken.Left)
                                    iBombOut += 1
                                    If iBombOut = 5000 Then
                                        badexp = False
                                        GoTo nxt
                                    End If
                                End While
                                badexp = False
                                GoTo nxt
                            Else
                                If bThrowExceptionOnBadExpression Then
                                    Throw New Exception("Bad expression: " & sExpression)
                                Else
                                    ErrMsg("Bad Expression: " & sExpression)
                                End If
                                Exit Sub
                            End If
                        End If
                        badexp = True
                    Else
                        ' Move token one to right
                        currtoken = Token(currtoken.Right)
                    End If
                End If

nxt:
            End While

            If Type = VariableTypeEnum.Numeric Then
                IntValue(iIndex) = SafeInt(Val(Token(iToken - 1).Value))
                'StringValue(iIndex) = IntValue(iIndex).ToString ' Used to evaluate datatype
                ' But we can't set indexed string value because it won't exist, only the int array...
            Else
                StringValue(iIndex) = Token(iToken - 1).Value
            End If

        Catch ex As Exception
            If bThrowExceptionOnBadExpression Then Throw New Exception("Error in SetToExpression for variable " & Me.Name, ex)
        End Try

    End Sub

    Private Sub addtoken(ByVal tokenv As String, ByVal value As String, ByVal left As Integer, ByVal right As Integer)

        ReDim Preserve Token(iToken + 1)

        With Token(iToken)
            .Token = tokenv
            .Value = value
            .Left = left
            .Right = right
        End With
        iToken = iToken + 1

    End Sub

    Private Function StripRedundantSpaces(ByVal strExpression As String) As String

        Dim bolOkToStrip As Boolean
        bolOkToStrip = True
        Dim strChunk As String
        Dim intPos As Integer

        StripRedundantSpaces = ""

        While strExpression <> ""
            intPos = sInstr(1, strExpression, Chr(34))
            If intPos > 0 Then
                strChunk = sLeft(strExpression, intPos)
                strExpression = sRight(strExpression, Len(strExpression) - intPos)
                If bolOkToStrip Then strChunk = Replace(strChunk, " ", "").Replace(vbCrLf, "").Replace(vbLf, "")
                bolOkToStrip = Not bolOkToStrip
            Else
                strChunk = strExpression
                If bolOkToStrip Then strChunk = Replace(strChunk, " ", "").Replace(vbCrLf, "").Replace(vbLf, "")
                strExpression = ""
            End If

            Debug.Print("Chunk: " & strChunk & " - " & bolOkToStrip)
            StripRedundantSpaces = StripRedundantSpaces & strChunk
        End While
    End Function


#End Region

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
        If Adventure.htblVariables.ContainsKey(sKey) AndAlso Me.StringValue.Contains("%" & Adventure.htblVariables(sKey).Name & "%") Then iCount += 1

        Return iCount
    End Function

    Public Overrides Function DeleteKey(ByVal sKey As String) As Boolean
        For Each d As Description In AllDescriptions
            If Not d.DeleteKey(sKey) Then Return False
        Next
        If Adventure.htblVariables.ContainsKey(sKey) AndAlso Me.StringValue.Contains("%" & Adventure.htblVariables(sKey).Name & "%") Then
            If Adventure.htblVariables(sKey).Type = VariableTypeEnum.Numeric Then
                StringValue = StringValue.Replace("%" & Adventure.htblVariables(sKey).Name & "%", "0")
            Else
                StringValue = StringValue.Replace("%" & Adventure.htblVariables(sKey).Name & "%", """""")
            End If
        End If
        Return True
    End Function

End Class
