Imports FrankenDrift.Glue

Public Class clsEvent
    Inherits clsItem
    Public Enum EventTypeEnum As Integer
        TurnBased = 0
        TimeBased = 1
    End Enum
    Public EventType As EventTypeEnum

    Public Enum WhenStartEnum
        Immediately = 1
        BetweenXandYTurns = 2
        AfterATask = 3
    End Enum
    Friend WhenStart As WhenStartEnum
    Friend arlEventDescriptions As EventDescriptionArrayList
    Private iTimerToEndOfEvent As Integer
    Friend iLastSubEventTime As Integer = 0
    Private stackLookText As New Stack
    Private Enum Command
        [Nothing] = 0
        Start = 1
        [Stop] = 2
        Pause = 3
        [Resume] = 4
    End Enum
    Private NextCommand As Command = Command.Nothing

    Private Class clsLookText
        Public sDescription As String
        Public sLocationKey As String

        Public Sub New(ByVal sKey As String, ByVal sDescription As String)
            Me.sLocationKey = sKey
            Me.sDescription = sDescription
        End Sub

    End Class

    Public Enum StatusEnum
        NotYetStarted = 0
        Running = 1
        CountingDownToStart = 2
        Paused = 3
        Finished = 4
    End Enum
    Friend Status As StatusEnum

    Friend EventControls() As EventOrWalkControl = {}

    Public Class SubEvent
        Implements ICloneable

        Private sParentKey As String
        Public Sub New(ByVal sEventKey As String)
            sParentKey = sEventKey
        End Sub


        Public Enum WhenEnum
            FromLastSubEvent
            FromStartOfEvent
            BeforeEndOfEvent
        End Enum
        Public eWhen As WhenEnum

        Public Enum WhatEnum
            DisplayMessage
            SetLook
            ExecuteTask
            UnsetTask
        End Enum
        Public eWhat As WhatEnum

        Public Enum MeasureEnum
            Turns
            Seconds
        End Enum
        Public eMeasure As MeasureEnum

        Friend ftTurns As New FromTo
        Friend oDescription As New Description
        Public sKey As String

        Private Function Clone() As Object Implements System.ICloneable.Clone
            Dim se As SubEvent
            se = CType(Me.MemberwiseClone, SubEvent)
            se.ftTurns = ftTurns.CloneMe
            se.oDescription = oDescription.Copy
            Return se
        End Function
        Public Function CloneMe() As SubEvent
            Return CType(Me.Clone, SubEvent)
        End Function

        Public WithEvents tmrTrigger As System.Timers.Timer
        Public dtStart As DateTime
        Public iMilliseconds As Integer
        Private Sub tmrTrigger_Tick(sender As Object, e As System.EventArgs) Handles tmrTrigger.Elapsed
            tmrTrigger.Enabled = False
            tmrTrigger.Stop()
            UserSession.TriggerTimerEvent(sParentKey, Me)
        End Sub

    End Class


    Public SubEvents() As SubEvent = {}

    Friend StartDelay As FromTo
    Friend Length As FromTo

    Public Property Description() As String
    Public Property Repeating() As Boolean
    Public Property RepeatCountdown As Boolean

    Public Sub New()
        Me.StartDelay = New FromTo
        Me.Length = New FromTo
    End Sub

    Public Overrides ReadOnly Property Clone() As clsItem
        Get
            Dim ev As clsEvent = CType(Me.MemberwiseClone, clsEvent)
            ev.StartDelay = ev.StartDelay.CloneMe
            ev.Length = ev.Length.CloneMe
            Return ev
        End Get
    End Property

    Public Function LookText() As String

        If Status = StatusEnum.Running Then

            ' Pop the first matching LookText off the stack
            Dim bOkToDisplay As Boolean = False
            Dim sLookText As String = Nothing

            For Each lt As clsLookText In stackLookText.ToArray
                If Adventure.Player.IsInGroupOrLocation(lt.sLocationKey) Then
                    sLookText = lt.sDescription
                    bOkToDisplay = True
                    Exit For
                End If
            Next

            If bOkToDisplay Then Return sLookText

        End If

        Return Nothing

    End Function


    Public Property TimerToEndOfEvent() As Integer
        Get
            Return iTimerToEndOfEvent
        End Get
        Set(ByVal value As Integer)
            Dim iStartValue As Integer = iTimerToEndOfEvent
            iTimerToEndOfEvent = value

            ' If the timer has ticked down and we're ready to start
            If Status = StatusEnum.CountingDownToStart AndAlso TimerFromStartOfEvent = 0 Then
                Start(True)
            End If

            ' If we've reached the end of the timer
            If Status = StatusEnum.Running AndAlso iTimerToEndOfEvent = 0 Then
                lStop(True)
            End If

        End Set
    End Property

    Friend LastSubEvent As SubEvent
    Private ReadOnly Property TimerFromLastSubEvent() As Integer
        Get
            Return TimerFromStartOfEvent - iLastSubEventTime
        End Get
    End Property
    Friend ReadOnly Property TimerFromStartOfEvent() As Integer
        Get
            Return Length.Value - TimerToEndOfEvent ' + 1
        End Get
    End Property


    Public sTriggeringTask As String


    Public bJustStarted As Boolean = False
    Public Sub Start(Optional ByVal bForce As Boolean = False)
        If bForce OrElse UserSession.bEventsRunning Then
            lStart()
        Else
            NextCommand = Command.Start
        End If
    End Sub
    Private Sub lStart(Optional ByVal bRestart As Boolean = False)
        If Status = StatusEnum.NotYetStarted OrElse Status = StatusEnum.CountingDownToStart OrElse Status = StatusEnum.Finished OrElse (Status = StatusEnum.Running AndAlso bRestart) Then
            If Not bRestart Then UserSession.DebugPrint(ItemEnum.Event, Me.Key, DebugDetailLevelEnum.Low, "Starting event " & Description)
            Status = StatusEnum.Running
            Length.Reset()

            LastSubEvent = Nothing
            iLastSubEventTime = 0

            For Each se As clsEvent.SubEvent In SubEvents
                se.ftTurns.Reset()
                If se.eMeasure = SubEvent.MeasureEnum.Seconds AndAlso EventType = EventTypeEnum.TurnBased Then
#If Not Adravalon Then
                    se.tmrTrigger = New System.Windows.Forms.Timer
#Else
                    se.tmrTrigger = New System.Timers.Timer
#End If
                    If se.eWhen = SubEvent.WhenEnum.FromStartOfEvent OrElse (se.eWhen = SubEvent.WhenEnum.FromLastSubEvent AndAlso se Is SubEvents(0)) Then
                        se.tmrTrigger.Interval = se.ftTurns.Value * 1000
                        se.tmrTrigger.Start()
                        se.dtStart = Now
                    End If
                End If
            Next

            ' WHAT TO DO HERE?
            ' If it's length 0, we need to run our start actions
            ' if it's length 2 we don't want it being set to 1 immediately from the incrementtimer
            TimerToEndOfEvent = Length.Value
            If TimerFromStartOfEvent = 0 Then DoAnySubEvents() ' To run 'after 0 turns' subevents

            If WhenStart = WhenStartEnum.Immediately Then WhenStart = WhenStartEnum.BetweenXandYTurns ' So we get 'after 0 turns' on any repeats
            bJustStarted = True
        Else
            'Throw New Exception("Can't Start an Event that isn't waiting!")
            UserSession.DebugPrint(ItemEnum.Event, Me.Key, DebugDetailLevelEnum.Error, "Can't Start an Event that isn't waiting!")
        End If
    End Sub


    Public Sub Pause()
        If UserSession.bEventsRunning Then lPause() Else NextCommand = Command.Pause
    End Sub
    Private Sub lPause()
        If Status = StatusEnum.Running Then
            UserSession.DebugPrint(ItemEnum.Event, Me.Key, DebugDetailLevelEnum.Low, "Pausing event " & Description)
            Status = StatusEnum.Paused
            For Each se As SubEvent In SubEvents
                If se.tmrTrigger IsNot Nothing AndAlso se.tmrTrigger.Enabled Then
                    se.iMilliseconds = CInt(se.ftTurns.Value * 1000 - Now.Subtract(se.dtStart).TotalMilliseconds)
                    se.tmrTrigger.Stop()
                End If
            Next
        Else
            UserSession.DebugPrint(ItemEnum.Event, Me.Key, DebugDetailLevelEnum.Error, "Can't Pause an Event that isn't running!")
        End If
    End Sub


    Public Sub [Resume]()
        If UserSession.bEventsRunning Then lResume() Else NextCommand = Command.Resume
    End Sub
    Private Sub lResume()
        If Status = StatusEnum.Paused Then
            UserSession.DebugPrint(ItemEnum.Event, Me.Key, DebugDetailLevelEnum.Low, "Resuming event " & Description)
            Status = StatusEnum.Running
            For Each se As SubEvent In SubEvents
                If se.tmrTrigger IsNot Nothing Then
                    If se.iMilliseconds > 0 Then
                        se.tmrTrigger.Interval = se.iMilliseconds
                        se.iMilliseconds = 0
                        se.tmrTrigger.Start()
                    End If
                End If
            Next
        Else
            UserSession.DebugPrint(ItemEnum.Event, Me.Key, DebugDetailLevelEnum.Error, "Can't Resume an Event that isn't paused!")
        End If
    End Sub


    Public Sub [Stop]()
        ' If an event runs a task and that task starts/stops an event, do it immediately
        If UserSession.bEventsRunning Then lStop() Else NextCommand = Command.Stop
    End Sub
    Private Sub lStop(Optional ByVal bRunSubEvents As Boolean = False)

        If bRunSubEvents Then DoAnySubEvents()
        If Status = StatusEnum.Paused Then Exit Sub
        Status = StatusEnum.Finished
        For Each se As SubEvent In SubEvents
            If se.tmrTrigger IsNot Nothing Then se.tmrTrigger.Stop()
        Next
        If Repeating AndAlso TimerToEndOfEvent = 0 Then
            If Length.Value > 0 Then
                UserSession.DebugPrint(ItemEnum.Event, Me.Key, DebugDetailLevelEnum.Low, "Restarting event " & Description)
                If RepeatCountdown Then
                    Status = clsEvent.StatusEnum.CountingDownToStart
                    StartDelay.Reset()
                    TimerToEndOfEvent = StartDelay.Value + Length.Value
                Else
                    lStart(True) ' Make sure we don't get ourselves in a loop for zero length events
                End If
            Else
                UserSession.DebugPrint(ItemEnum.Event, Me.Key, DebugDetailLevelEnum.Low, "Not restarting event " & Description & " otherwise we'd get in an infinite loop as zero length.")
            End If
        Else
            UserSession.DebugPrint(ItemEnum.Event, Me.Key, DebugDetailLevelEnum.Low, "Finishing event " & Description)
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
            End Select
            NextCommand = Command.Nothing
            sTriggeringTask = ""
        End If

        If Status = StatusEnum.Running OrElse Status = StatusEnum.CountingDownToStart Then UserSession.DebugPrint(ItemEnum.Event, Key, DebugDetailLevelEnum.High, "Event " & Description & " [" & TimerFromStartOfEvent + 1 & "/" & Length.Value & "]")

        ' Split this into 2 case statements, as changing timer here may change status
        Select Case Status
            Case StatusEnum.NotYetStarted
            Case StatusEnum.CountingDownToStart
                TimerToEndOfEvent -= 1
            Case StatusEnum.Running
                If Not bJustStarted Then TimerToEndOfEvent -= 1
            Case StatusEnum.Paused
            Case StatusEnum.Finished
        End Select

        If Not bJustStarted Then DoAnySubEvents()

        bJustStarted = False

    End Sub

    Friend Sub DoAnySubEvents()

        Select Case Status
            Case StatusEnum.Running
                ' Check all the subevents to see if we need to do anything
                Dim iIndex As Integer = 0
                For Each se As SubEvent In SubEvents
                    If se.eMeasure = SubEvent.MeasureEnum.Turns OrElse EventType = EventTypeEnum.TimeBased Then
                        Dim bRunSubEvent As Boolean = False
                        Select Case se.eWhen
                            Case SubEvent.WhenEnum.FromStartOfEvent
                                If TimerFromStartOfEvent = se.ftTurns.Value AndAlso se.ftTurns.Value <= Length.Value AndAlso (se.ftTurns.Value > 0 OrElse Me.WhenStart <> WhenStartEnum.Immediately) Then bRunSubEvent = True
                            Case SubEvent.WhenEnum.FromLastSubEvent
                                If TimerFromLastSubEvent = se.ftTurns.Value Then
                                    If (LastSubEvent Is Nothing AndAlso iIndex = 0) OrElse (iIndex > 0 AndAlso LastSubEvent Is SubEvents(iIndex - 1)) Then bRunSubEvent = True
                                End If
                            Case SubEvent.WhenEnum.BeforeEndOfEvent
                                If TimerToEndOfEvent = se.ftTurns.Value Then bRunSubEvent = True
                        End Select

                        If bRunSubEvent Then RunSubEvent(se)

                    End If
                    iIndex += 1
                Next
        End Select

    End Sub


    Public Sub RunSubEvent(ByVal se As SubEvent)

        Select Case se.eWhat
            Case SubEvent.WhatEnum.DisplayMessage
                If se.sKey <> "" AndAlso Adventure.Player.IsInGroupOrLocation(se.sKey) Then UserSession.Display(se.oDescription.ToString)
            Case SubEvent.WhatEnum.ExecuteTask
                If Adventure.htblTasks.ContainsKey(se.sKey) Then
                    UserSession.DebugPrint(ItemEnum.Event, Key, DebugDetailLevelEnum.Medium, "Event '" & Description & "' attempting to execute task '" & Adventure.htblTasks(se.sKey).Description & "'")
                    UserSession.AttemptToExecuteTask(se.sKey, True, , , , , , , True)
                End If
            Case SubEvent.WhatEnum.SetLook
                ' Push a LookText onto the stack                                
                stackLookText.Push(New clsLookText(se.sKey, se.oDescription.ToString))
            Case SubEvent.WhatEnum.UnsetTask
                UserSession.DebugPrint(ItemEnum.Event, Key, DebugDetailLevelEnum.Medium, "Event '" & Description & "' unsetting task '" & Adventure.htblTasks(se.sKey).Description & "'")
                Adventure.htblTasks(se.sKey).Completed = False
        End Select
        iLastSubEventTime = TimerFromStartOfEvent
        LastSubEvent = se

        Dim i As Integer = 0
        For Each ose As SubEvent In SubEvents
            i += 1
            If ose Is se Then
                If i < SubEvents.Length Then
                    With SubEvents(i)
                        If .eMeasure = SubEvent.MeasureEnum.Seconds AndAlso EventType = EventTypeEnum.TurnBased Then
                            If .eWhen = SubEvent.WhenEnum.FromLastSubEvent Then
                                .tmrTrigger = New System.Timers.Timer
                                If .ftTurns.Value > 0 Then
                                    .tmrTrigger.Interval = .ftTurns.Value * 1000
                                    .tmrTrigger.Start()
                                    .dtStart = Now
                                Else
                                    RunSubEvent(SubEvents(i))
                                End If
                            End If
                        End If
                    End With
                    Exit For
                End If
            End If
        Next
    End Sub


    Public Overrides ReadOnly Property CommonName() As String
        Get
            Return Description
        End Get
    End Property


    Friend Overrides ReadOnly Property AllDescriptions() As Generic.List(Of SharedModule.Description)
        Get
            Dim all As New Generic.List(Of Description)
            For Each se As SubEvent In SubEvents
                all.Add(se.oDescription)
            Next
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
        For i As Integer = 0 To EventControls.Length - 1
            If EventControls(i).sTaskKey = sKey Then iCount += 1
        Next
        For i As Integer = 0 To SubEvents.Length - 1
            With SubEvents(i)
                iCount += .oDescription.ReferencesKey(sKey)
                If .sKey = sKey Then iCount += 1
            End With
        Next
        Return iCount

    End Function


    Public Overrides Function DeleteKey(ByVal sKey As String) As Boolean

        For i As Integer = EventControls.Length - 1 To 0 Step -1
            If EventControls(i).sTaskKey = sKey Then
                For j As Integer = EventControls.Length - 2 To i Step -1
                    EventControls(j) = EventControls(j + 1)
                Next
                ReDim Preserve EventControls(EventControls.Length - 2)
            End If
        Next
        For i As Integer = 0 To SubEvents.Length - 1
            With SubEvents(i)
                If Not .oDescription.DeleteKey(sKey) Then Return False
                If .sKey = sKey Then .sKey = ""
            End With
        Next

        Return True

    End Function

End Class


Public Class clsEventDescription

    Friend arlShowInRooms As New StringArrayList
    Public Enum WhenShowEnum
        EventStart
        WhenPlayerLooks
        XTurnsFromStart
        XTurnsFromFinish
        EventFinish
    End Enum
    Friend WhenShow As WhenShowEnum

End Class