import QtQuick
import QtQuick.Controls

// Native Qt Quick version of the standalone frontend design. Game launch and
// player connections are demonstrations until connected to the Dolphin model.
ApplicationWindow {
    id: window
    width: 1280
    height: 720
    minimumWidth: 800
    minimumHeight: 500
    visible: true
    title: "WiiTogether"
    color: "#e4e7e9"
    font.family: wiiFont.name

    FontLoader { id: wiiFont; source: "wiitogether-assets/main.ttf" }
    property int selectedGame: 2
    property int openPlayer: -1
    property var connectedPlayers: [false, false, false, false]
    property bool pointerEnabled: true
    property bool animationsEnabled: true
    property string clockText: Qt.formatTime(new Date(), "h:mm ap")
    Timer { interval: 30000; running: true; repeat: true; onTriggered: window.clockText = Qt.formatTime(new Date(), "h:mm ap") }
    readonly property var playerColors: ["#27a9e3", "#ec6370", "#70b65b", "#e3ad33"]
    readonly property var games: [
        { title: "Wii Sports Resort", detail: "Sports  •  1–4 players", cover: "RZTE01.png" },
        { title: "Super Mario Galaxy", detail: "Adventure  •  1–2 players", cover: "RMGE01.png" },
        { title: "Mario Kart Wii", detail: "Racing  •  1–4 players", cover: "RMCE01.png" },
        { title: "Super Smash Bros. Brawl", detail: "Fighting  •  1–4 players", cover: "RSBE01.png" },
        { title: "Wii Party", detail: "Party  •  1–4 players", cover: "SUPE01.png" }
    ]
    readonly property int playerCount: connectedPlayers.filter(function(value) { return value }).length
    function browse(direction) { selectedGame = (selectedGame + direction + games.length) % games.length }
    function togglePlayer(index) {
        let next = connectedPlayers.slice()
        next[index] = !next[index]
        connectedPlayers = next
    }

    component WiiButton: AbstractButton {
        id: control
        implicitWidth: 170
        implicitHeight: 48
        hoverEnabled: true
        scale: window.animationsEnabled && hovered ? 1.025 : 1
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        font.family: wiiFont.name
        font.pixelSize: 21
        background: Rectangle {
            radius: height / 2
            border.width: 2
            border.color: control.hovered || control.activeFocus ? "#37bae8" : "#bdc6cc"
            gradient: Gradient {
                GradientStop { position: 0; color: "#ffffff" }
                GradientStop { position: 0.5; color: "#f7f9fa" }
                GradientStop { position: 1; color: control.down ? "#d5eaf3" : "#dfe5e9" }
            }
            Rectangle { anchors.fill: parent; anchors.margins: 4; radius: height / 2; color: "transparent"; border.color: "#ffffff"; border.width: 2 }
        }
        contentItem: Text {
            text: control.text
            font: control.font
            color: "#7d8b95"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        HoverHandler { cursorShape: window.pointerEnabled ? Qt.BlankCursor : Qt.PointingHandCursor }
    }

    Item {
        id: canvas
        width: 1280
        height: 720
        anchors.centerIn: parent
        scale: Math.min(window.width / width, window.height / height)
        focus: true
        Keys.onLeftPressed: window.browse(-1)
        Keys.onRightPressed: window.browse(1)
        Keys.onReturnPressed: launchDialog.open()
        Keys.onEscapePressed: window.openPlayer = -1

        Image { anchors.fill: parent; source: "wiitogether-assets/bg.png"; fillMode: Image.Tile }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: "#aaffffff" }
                GradientStop { position: 0.6; color: "#d9ffffff" }
                GradientStop { position: 1; color: "#00ffffff" }
            }
        }
        Image { x: 0; y: 0; width: parent.width; height: 110; source: "wiitogether-assets/header-swoop.svg" }
        WiiButton { x: 58; y: 75; width: 145; text: "⏻  Quit"; onClicked: quitDialog.open() }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 16
            spacing: 11
            Image { source: "wiitogether-assets/wii-logo.svg"; width: 82; height: 39; fillMode: Image.PreserveAspectFit; opacity: 0.85 }
            Text { text: "Together"; font.family: wiiFont.name; font.pixelSize: 35; color: "#969b9e" }
        }
        Text { anchors.right: parent.right; anchors.rightMargin: 58; y: 14; text: window.clockText; font.family: wiiFont.name; font.pixelSize: 18; color: "#909ba2" }
        WiiButton { x: 1070; y: 75; width: 153; text: "⚙  Settings"; onClicked: settingsDialog.open() }

        Item {
            id: carousel
            x: 95; y: 160; width: 1090; height: 300
            Repeater {
                model: window.games
                delegate: Item {
                    id: cover
                    required property int index
                    required property var modelData
                    readonly property int offset: {
                        let value = (index - window.selectedGame + window.games.length) % window.games.length
                        return value > 2 ? value - window.games.length : value
                    }
                    width: 200
                    height: 282
                    x: carousel.width / 2 - width / 2 + offset * 223
                    y: 5
                    SequentialAnimation on y {
                        running: window.animationsEnabled && window.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 1; duration: 3000 + cover.index * 110; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 5; duration: 3000 + cover.index * 110; easing.type: Easing.InOutSine }
                    }
                    z: 5 - Math.abs(offset)
                    scale: offset === 0 ? 1 : Math.abs(offset) === 1 ? 0.82 : 0.67
                    opacity: offset === 0 ? 1 : 1 - Math.abs(offset) * 0.16
                    transform: Rotation { origin.x: cover.width / 2; origin.y: cover.height / 2; axis.y: 1; axis.x: 0; axis.z: 0; angle: cover.offset < 0 ? 25 : cover.offset > 0 ? -25 : 0 }
                    Behavior on x { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 300 } }
                    Image { x: -20; y: parent.height + 15 - cover.y; width: parent.width + 40; height: 24; source: "wiitogether-assets/cover-shadow.svg" }
                    Rectangle {
                        anchors.fill: parent; anchors.margins: -8; radius: 12; visible: cover.offset === 0; color: "transparent"; border.width: 1; border.color: "#7753c7ed"
                        SequentialAnimation on opacity {
                            running: window.animationsEnabled && cover.offset === 0 && window.visible
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.45; duration: 2000; easing.type: Easing.InOutSine }
                            NumberAnimation { to: 1; duration: 2000; easing.type: Easing.InOutSine }
                        }
                    }
                    Rectangle { anchors.fill: parent; anchors.margins: -3; radius: 8; color: "#ffffff"; border.width: cover.offset === 0 ? 3 : 1; border.color: cover.offset === 0 ? "#55c6ec" : "#c5cbd0" }
                    Image { anchors.fill: parent; source: "wiitogether-assets/" + cover.modelData.cover; fillMode: Image.PreserveAspectFit; smooth: true }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: window.pointerEnabled ? Qt.BlankCursor : Qt.PointingHandCursor
                        onClicked: { if (window.selectedGame === cover.index) launchDialog.open(); else window.selectedGame = cover.index }
                    }
                }
            }
        }
        Repeater {
            model: [-1, 1]
            delegate: AbstractButton {
                required property int modelData
                x: modelData === -1 ? 45 : 1189
                y: 270
                width: 46; height: 66
                hoverEnabled: true
                contentItem: Image {
                    source: "wiitogether-assets/" + (parent.modelData === -1 ? (parent.hovered ? "backhover.png" : "back.png") : (parent.hovered ? "forwardhover.png" : "forward.png"))
                    fillMode: Image.PreserveAspectFit
                }
                onClicked: window.browse(modelData)
                HoverHandler { cursorShape: window.pointerEnabled ? Qt.BlankCursor : Qt.PointingHandCursor }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 503; spacing: 10
            Repeater { model: window.games.length; delegate: Rectangle { required property int index; width: 8; height: 8; radius: 4; color: index === window.selectedGame ? "#44bee8" : "#c6cdd2" } }
        }
        WiiButton { anchors.horizontalCenter: parent.horizontalCenter; y: 526; width: 236; height: 49; text: "Start Game"; onClicked: launchDialog.open() }

        Text { x: 65; y: 598; text: window.playerCount ? window.playerCount + " players ready to play" : "Connect a phone to play together"; font.family: wiiFont.name; font.pixelSize: 14; color: "#8a969f" }
        Row {
            x: 57; y: 626; spacing: 13; z: 20
            Repeater {
                model: 4
                delegate: Item {
                    id: player
                    required property int index
                    width: 281
                    height: 65
                    readonly property bool connected: window.connectedPlayers[index]
                    readonly property color tint: window.playerColors[index]
                    Rectangle {
                        anchors.fill: parent; radius: 14
                        border.width: 2
                        border.color: player.connected ? player.tint : cardHover.hovered ? "#43bde8" : "#bac4cc"
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#ffffff" }
                            GradientStop { position: 1; color: player.connected ? Qt.tint("#e4e9ed", Qt.rgba(player.tint.r, player.tint.g, player.tint.b, 0.25)) : "#dfe3e8" }
                        }
                        Rectangle { anchors.fill: parent; anchors.margins: 4; color: "transparent"; border.color: "#eeffffff"; radius: 10; border.width: 2 }
                        Text { x: 22; y: 10; text: "Player " + (player.index + 1); color: player.connected ? player.tint : "#a2adb6"; font.family: wiiFont.name; font.pixelSize: 21 }
                        Text { x: 22; y: 37; text: player.connected ? "Ready to play" : "Connect a phone"; color: "#9aa7b0"; font.family: wiiFont.name; font.pixelSize: 12 }
                        Rectangle {
                            x: 20; y: 58; width: parent.width - 40; height: 3; radius: 2; color: player.connected ? player.tint : "#c6ced5"
                            Behavior on color { ColorAnimation { duration: 220 } }
                            SequentialAnimation on opacity {
                                running: player.connected && window.animationsEnabled && window.visible
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.65; duration: 1900; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1; duration: 1900; easing.type: Easing.InOutSine }
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: window.pointerEnabled ? Qt.BlankCursor : Qt.PointingHandCursor
                        onClicked: window.openPlayer = window.openPlayer === player.index ? -1 : player.index
                    }
                    HoverHandler { id: cardHover; onHoveredChanged: if (hovered) window.openPlayer = player.index }
                    Timer { interval: 170; running: window.openPlayer === player.index && !cardHover.hovered && !panelHover.hovered; onTriggered: window.openPlayer = -1 }
                    Rectangle {
                        id: qrPanel
                        visible: window.openPlayer === player.index
                        x: (parent.width - width) / 2
                        y: -325
                        width: 235; height: 310; radius: 21; color: "#f8fbfd"; border.width: 2; border.color: "#51c4ea"
                        HoverHandler { id: panelHover }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; y: 18; text: "Let's play together."; font.family: wiiFont.name; font.pixelSize: 19; color: "#81939f" }
                        Image { anchors.horizontalCenter: parent.horizontalCenter; y: 51; width: 160; height: 160; source: "wiitogether-assets/join-preview.svg" }
                        WiiButton { anchors.horizontalCenter: parent.horizontalCenter; y: 226; width: 195; height: 33; font.pixelSize: 14; text: player.connected ? "Preview disconnect" : "Preview connection"; onClicked: window.togglePlayer(player.index) }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; y: 280; text: "Demo QR · live connection in Dolphin"; font.family: wiiFont.name; font.pixelSize: 10; color: "#a2aeb6" }
                    }
                }
            }
        }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: 701; text: "Ⓐ Select      ·      Point at a player to connect"; font.family: wiiFont.name; font.pixelSize: 12; color: "#96a2ac" }
        HoverHandler { id: pointerTracker; acceptedDevices: PointerDevice.Mouse; cursorShape: window.pointerEnabled ? Qt.BlankCursor : Qt.ArrowCursor }
        Image {
            x: pointerTracker.point.position.x - 19
            y: pointerTracker.point.position.y - 3
            width: 48; height: 52
            z: 999
            visible: window.pointerEnabled && pointerTracker.hovered
            source: "wiitogether-assets/cursor.png"
            fillMode: Image.PreserveAspectFit
        }
    }

    Dialog {
        id: settingsDialog
        anchors.centerIn: parent
        width: 450; title: "WiiTogether Settings"; modal: true
        standardButtons: Dialog.Close
        Column {
            spacing: 16
            CheckBox { text: "Player 1 pointer"; checked: window.pointerEnabled; onToggled: window.pointerEnabled = checked }
            CheckBox { text: "Menu animations"; checked: window.animationsEnabled; onToggled: window.animationsEnabled = checked }
            Label { text: "Preview player connections"; color: "#8e9da7" }
            Repeater { model: 4; delegate: CheckBox { required property int index; text: "Player " + (index + 1); checked: window.connectedPlayers[index]; onToggled: window.togglePlayer(index) } }
        }
    }
    Dialog {
        id: quitDialog
        anchors.centerIn: parent
        title: "Finished playing?"; modal: true
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: Qt.quit()
        Label { text: "Would you like to close WiiTogether?" }
    }
    Dialog {
        id: launchDialog
        anchors.centerIn: parent
        width: 490; title: window.games[window.selectedGame].title; modal: true
        standardButtons: Dialog.Close
        Label { width: parent.width; wrapMode: Text.WordWrap; text: "Your game is selected. This frontend preview does not launch Dolphin or connect phones yet." }
    }
}
