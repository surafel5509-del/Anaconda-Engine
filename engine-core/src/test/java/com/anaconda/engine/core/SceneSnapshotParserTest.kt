// ============================================================================
//  SceneSnapshotParserTest.kt — JVM unit tests for the engine→editor line
//  protocol (Engine::sceneSnapshot / EditorSnapshotCodec 8-column layout).
//
//  Runs on the JVM with no Android/device dependency:
//      ./gradlew :engine-core:testDebugUnitTest        (wired into CI)
//
//  The codec is the single source of truth for the wire format; these tests
//  pin the CONTRACT so native refactors break the build, not the editor.
// ============================================================================
package com.anaconda.engine.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SceneSnapshotParserTest {

    private val heroLine =
        "1|Hero|1.5000,-2.2500,0.0000|0.0000,45.0000,0.0000|" +
            "1.0000,1.0000,1.0000|7|1.0000,0.5000,0.2500,1.0000|261"

    @Test
    fun parsesFullEightColumnLine() {
        val rows = parseSceneSnapshot(heroLine)
        assertEquals(1, rows.size)
        val hero = rows[0]
        assertEquals(1, hero.id)
        assertEquals("Hero", hero.name)
        assertEquals(1.5f, hero.position.x, 1e-4f)
        assertEquals(-2.25f, hero.position.y, 1e-4f)
        assertEquals(45f, hero.rotationDeg.y, 1e-3f)
        assertTrue(hero.hasSprite)
        assertTrue(hero.hasRigidBody)
        assertTrue(hero.hasCollider)
        assertEquals(0.5f, hero.spriteColor.y, 1e-4f)
        assertEquals(261, hero.textureHandle)
    }

    @Test
    fun toleratesMissingTextureColumn() {
        // Pre-Month-2 snapshots had exactly 7 columns — must still parse.
        val legacy = "2|Old|0,0,0|0,0,0|1,1,1|1|1,1,1,1"
        val rows = parseSceneSnapshot(legacy)
        assertEquals(1, rows.size)
        assertEquals(0, rows[0].textureHandle)
        assertTrue(rows[0].hasSprite)
        assertFalse(rows[0].hasCollider)
    }

    @Test
    fun textureZeroMeansUntextured() {
        val rows = parseSceneSnapshot(heroLine.replace("|261", "|0"))
        assertEquals(0, rows[0].textureHandle)
    }

    @Test
    fun flagsDecodeComponentBits() {
        val spriteOnly = "3|S|0,0,0|0,0,0|1,1,1|1|1,1,1,1|0"
        val rigidOnly = "4|R|0,0,0|0,0,0|1,1,1|2|1,1,1,1|0"
        val colliderOnly = "5|C|0,0,0|0,0,0|1,1,1|4|1,1,1,1|0"
        val none = "6|N|0,0,0|0,0,0|1,1,1|0|1,1,1,1|0"
        val rows = parseSceneSnapshot(
            listOf(spriteOnly, rigidOnly, colliderOnly, none).joinToString("\n"),
        )
        assertEquals(4, rows.size)
        assertTrue(rows[0].hasSprite && !rows[0].hasRigidBody && !rows[0].hasCollider)
        assertTrue(rows[1].hasRigidBody && !rows[1].hasSprite)
        assertTrue(rows[2].hasCollider && !rows[2].hasSprite)
        assertFalse(rows[3].hasSprite || rows[3].hasRigidBody || rows[3].hasCollider)
    }

    @Test
    fun skipsMalformedLinesWithoutThrowing() {
        val raw = listOf(
            "not|enough|columns",
            "x|Bad|a,b,c|0,0,0|1,1,1|1|1,1,1,1|0",   // id not a number
            heroLine,                                   // good line survives
            "9|Bad|0,0,0|0,0,0|1,1,1|7|1,1,1",         // color has 3 comps
        ).joinToString("\n")
        val rows = parseSceneSnapshot(raw)
        assertEquals(1, rows.size)
        assertEquals("Hero", rows[0].name)
    }

    @Test
    fun emptyAndBlankRawYieldNoRows() {
        assertTrue(parseSceneSnapshot("").isEmpty())
        assertTrue(parseSceneSnapshot("\n\n   \n").isEmpty())
    }

    @Test
    fun multiLineSnapshotPreservesOrderAndIds() {
        val raw = heroLine + "\n" + "7|Enemy|0,0,0|0,0,0|2,2,2|3|0.9,0.1,0.1,1|0"
        val rows = parseSceneSnapshot(raw)
        assertEquals(listOf(1, 7), rows.map { it.id })
        assertEquals(listOf("Hero", "Enemy"), rows.map { it.name })
        assertEquals(2f, rows[1].scale.x, 1e-4f)
    }

    @Test
    fun mathConversionsRoundTrip() {
        val rad = MathConversions.degToRad(90f)
        assertEquals(Math.PI.toFloat() / 2f, rad, 1e-5f)
        assertEquals(90f, MathConversions.radToDeg(rad), 1e-4f)
    }
}
