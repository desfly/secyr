package ua.homeguard.s3.ui.infrastructure

import ua.homeguard.s3.model.BuildInfoResponse

object BuildInfoFormatter {
    fun title(info: BuildInfoResponse): String =
        "${info.project} ${info.version} (Build-${info.build})"

    fun platform(info: BuildInfoResponse): String =
        "${info.board} / ${info.module}"

    fun provenance(info: BuildInfoResponse): String =
        "Git ${info.gitRevision}, ${info.buildTimestampUtc}, ESP-IDF ${info.espIdfRequired}"
}
