/*
 * Copyright (C) 2022 UnionTech Technology Co., Ltd.
 *
 * Author:     yeshanshan <yeshanshan@uniontech.com>
 *
 * Maintainer: yeshanshan <yeshanshan@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.11
import QtQuick.Layouts 1.11
import org.deepin.dtk.impl 1.0 as D
import org.deepin.dtk.style 1.0 as DS

FocusScope {
    id: control
    property string text
    property string alertText
    property int alertDuration
    property bool showAlert
    property D.Palette backgroundColor: DS.Style.editBackground

    width: impl.width
    height: impl.height

    Control {
        id: impl
        anchors.fill: parent
        contentItem: RowLayout {
            Repeater {
                id: fields
                model: 4
                Layout.fillWidth: true
                delegate: TextInput {
                    KeyNavigation.right: index < fields.count - 1 ? fields.itemAt(index + 1) : null
                    KeyNavigation.left: index > 0 ? fields.itemAt(index - 1) : null
                    selectByMouse: true
                    color: impl.palette.text
                    selectionColor: impl.palette.highlight
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData
                    Layout.preferredWidth: DS.Style.ipLineEdit.fieldWidth
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                    validator: RegExpValidator {
                        regExp: /^(([0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])?)$/
                    }
                    Label {
                        text: "."
                        anchors {
                            left: parent.right
                        }
                        visible: index < fields.count - 1
                    }
                    onTextEdited: fields.updateText()
                }
                function updateText() {
                    var text = ""
                    for (var i = 0; i < fields.count; ++i) {
                        text += fields.itemAt(i).text
                        if (i < fields.count - 1)
                            text += "."
                    }
                    control.text = text == "..." ? "" : text
                }
                function clearText() {
                    for (var i = 0; i < fields.count; ++i) {
                        fields.itemAt(i).text = ""
                    }
                    control.text = ""
                }
                function updateByText() {
                    if (control.text === "")
                        clearText()
                    var arrs = control.text.split(".")
                    if (arrs.length != 4)
                        return
                    fields.model = arrs
                }

                Component.onCompleted: {
                    updateByText()
                    clearBtn.clicked.connect(clearText)
                    control.textChanged.connect(updateByText)
                }
            }
            Item {
                width: height
                height: 36
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                ActionButton {
                    id: clearBtn
                    anchors.fill: parent
                    icon.name: "window-close_round"
                    visible: control.activeFocus && control.text
                    focusPolicy: Qt.NoFocus
                }
            }
        }

        background: EditPanel {
            control: impl
            alertText: control.alertText
            alertDuration: control.alertDuration
            showAlert: control.showAlert
            showBorder: control.activeFocus
            backgroundColor: control.backgroundColor
            implicitWidth: DS.Style.edit.width
            implicitHeight: DS.Style.edit.textFieldHeight
        }
    }
}